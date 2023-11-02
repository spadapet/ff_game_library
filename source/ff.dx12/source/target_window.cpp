#include "pch.h"
#include "commands.h"
#include "descriptor_allocator.h"
#include "device_reset_priority.h"
#include "globals.h"
#include "target_window.h"
#include "texture_util.h"
#include "queue.h"

static const size_t MIN_BUFFER_COUNT = 2;
static const size_t MAX_BUFFER_COUNT = 4;

static ff::perf_counter perf_render_wait("Wait", ff::perf_color::cyan, ff::perf_chart_t::render_wait);
static ff::perf_counter perf_render_present("Present", ff::perf_color::cyan, ff::perf_chart_t::render_wait);

static size_t fix_buffer_count(size_t value)
{
    return ff::math::clamp<size_t>(value, MIN_BUFFER_COUNT, MAX_BUFFER_COUNT);
}

static size_t fix_frame_latency(size_t value, size_t buffer_count)
{
    return ff::math::clamp<size_t>(value, 0, ::fix_buffer_count(buffer_count));
}

ff::dx12::target_window::target_window(ff::window* window, size_t buffer_count, size_t frame_latency, bool vsync, bool allow_full_screen)
    : window(window)
    , main_window(ff::window::main() == window)
    , window_message_connection(window->message_sink().connect(std::bind(&target_window::handle_message, this, std::placeholders::_1)))
    , vsync_(vsync)
    , allow_full_screen_(main_window&& allow_full_screen)
    , target_views(ff::dx12::cpu_target_descriptors().alloc_range(MAX_BUFFER_COUNT))
{
    this->internal_size(this->window->size(), ::fix_buffer_count(buffer_count), ::fix_frame_latency(frame_latency, buffer_count));

    ff::dx12::add_device_child(this, ff::dx12::device_reset_priority::target_window);

    if (this->allow_full_screen_)
    {
        ff::dxgi_host().full_screen_target(this);
    }
}

ff::dx12::target_window::~target_window()
{
    ff::dx12::wait_for_idle();

    if (this->allow_full_screen_ && this->swap_chain)
    {
        this->swap_chain->SetFullscreenState(FALSE, nullptr);
    }

    ff::dxgi_host().remove_target(this);
    ff::dx12::remove_device_child(this);
}

ff::dx12::target_window::operator bool() const
{
    return this->swap_chain &&
        this->window &&
        this->target_textures.size() > this->back_buffer_index &&
        this->target_textures[this->back_buffer_index];
}

DXGI_FORMAT ff::dx12::target_window::format() const
{
    return DXGI_FORMAT_B8G8R8A8_UNORM;
}

ff::window_size ff::dx12::target_window::size() const
{
    return this->cached_size;
}

ff::dx12::resource& ff::dx12::target_window::dx12_target_texture()
{
    return *this->target_textures[this->back_buffer_index];
}

D3D12_CPU_DESCRIPTOR_HANDLE ff::dx12::target_window::dx12_target_view()
{
    return this->target_views.cpu_handle(this->back_buffer_index);
}

void ff::dx12::target_window::clear(ff::dxgi::command_context_base& context, const DirectX::XMFLOAT4& clear_color)
{
    ff::dx12::commands::get(context).clear(*this, clear_color);
}

bool ff::dx12::target_window::begin_render(ff::dxgi::command_context_base& context, const DirectX::XMFLOAT4* clear_color)
{
    if (*this && ff::dx12::device_valid())
    {
        if (clear_color)
        {
            this->clear(context, *clear_color);
        }
        else
        {
            ff::dx12::commands::get(context).discard(*this);
        }

        return true;
    }

    return false;
}

bool ff::dx12::target_window::end_render(ff::dxgi::command_context_base& context)
{
    if (*this && ff::dx12::device_valid())
    {
        if (this->frame_latency_handle)
        {
            ff::perf_timer timer(::perf_render_wait);
            this->frame_latency_handle.wait(INFINITE, false);
        }

        ff::perf_timer timer(::perf_render_present);
        ff::dx12::commands& commands = ff::dx12::commands::get(context);
        commands.resource_state(*this->target_textures[this->back_buffer_index], D3D12_RESOURCE_STATE_PRESENT);
        commands.queue().execute(commands);

        HRESULT hr = this->swap_chain->Present(this->vsync_ ? 1 : 0, 0);
        if (hr != DXGI_ERROR_DEVICE_RESET && hr != DXGI_ERROR_DEVICE_REMOVED)
        {
            this->back_buffer_index = static_cast<size_t>(this->swap_chain->GetCurrentBackBufferIndex());
            return true;
        }
    }

    return false;
}

void ff::dx12::target_window::before_resize()
{
    ff::dx12::wait_for_idle();
    this->before_reset();
}

bool ff::dx12::target_window::internal_reset(size_t buffer_count, size_t frame_latency)
{
    assert_ret_val(this->window, false);

    BOOL full_screen{};
#if !UWP_APP
    if (this->allow_full_screen_ && this->swap_chain)
    {
        Microsoft::WRL::ComPtr<IDXGIOutput> output;
        this->swap_chain->GetFullscreenState(&full_screen, &output);
        this->swap_chain->SetFullscreenState(FALSE, nullptr);
    }
#endif

    this->before_resize();
    this->swap_chain.Reset();

    ff::window_size size = this->window->size();
    assert_ret_val(this->internal_size(size, buffer_count, frame_latency), false);

    if (this->allow_full_screen_ && full_screen)
    {
        this->swap_chain->SetFullscreenState(TRUE, nullptr);
    }

    return *this;
}

ff::dxgi::target_access_base& ff::dx12::target_window::target_access()
{
    return *this;
}

size_t ff::dx12::target_window::target_array_start() const
{
    return 0;
}

size_t ff::dx12::target_window::target_array_size() const
{
    return 1;
}

size_t ff::dx12::target_window::target_mip_start() const
{
    return 0;
}

size_t ff::dx12::target_window::target_mip_size() const
{
    return 1;
}

size_t ff::dx12::target_window::target_sample_count() const
{
    return 1;
}

bool ff::dx12::target_window::size(const ff::window_size& size)
{
    return this->internal_size(size, this->buffer_count(), this->frame_latency());
}

bool ff::dx12::target_window::internal_size(const ff::window_size& size, size_t buffer_count, size_t frame_latency)
{
    if (!this->window || !this->window_message_connection)
    {
        // Could be called while the window is being destroyed in full screen mode
        return false;
    }

    if (this->swap_chain && (this->frame_latency() == 0) != (frame_latency == 0)) // on game thread
    {
        // Turn frame latency on/off requires recreating everything
        if (!this->internal_reset(buffer_count, frame_latency))
        {
            ff::dx12::device_fatal_error("Swap chain failed to change frame latency");
            return false;
        }

        return true;
    }

    ff::log::write(ff::log::type::dx12_target,
        "Swap chain set size.",
        " Size=", size.logical_pixel_size.x, ",", size.logical_pixel_size.y,
        " Rotate=", size.rotated_degrees(),
        " Buffers=", buffer_count,
        " Latency=", frame_latency,
        " VSync=", this->vsync_);

    ff::window_size old_size = this->cached_size;
    ff::point_t<UINT> buffer_size = size.physical_pixel_size().cast<UINT>();
    this->cached_size = size;
#if UWP_APP
    this->cached_full_screen_uwp = false;
#endif

    this->before_resize();

    if (this->swap_chain && (old_size != size || this->buffer_count() != buffer_count)) // on game thread
    {
        ff::log::write(ff::log::type::dx12_target, "- Resize existing buffers");

        DXGI_SWAP_CHAIN_DESC1 desc;
        if (FAILED(this->swap_chain->GetDesc1(&desc)) || FAILED(this->swap_chain->ResizeBuffers(
            static_cast<UINT>(buffer_count), buffer_size.x, buffer_size.y, desc.Format, desc.Flags)))
        {
            ff::dx12::device_fatal_error("Swap chain resize failed");
            return false;
        }
    }
    else if (!this->swap_chain) // first init on UI thread, reset is on game thread
    {
        ff::log::write(ff::log::type::dx12_target, "- Creating new swap chain");

        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Width = buffer_size.x;
        desc.Height = buffer_size.y;
        desc.Format = this->format();
        desc.SampleDesc.Count = 1;
        desc.BufferCount = static_cast<UINT>(buffer_count);
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.Scaling = DXGI_SCALING_NONE;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        desc.Flags = frame_latency ? DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT : 0;

        Microsoft::WRL::ComPtr<IDXGISwapChain1> new_swap_chain;
        Microsoft::WRL::ComPtr<IDXGIFactory2> factory = ff::dx12::factory();
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> command_queue = ff::dx12::get_command_queue(ff::dx12::direct_queue());

        ff::thread_dispatch::get_main()->send([this, factory, command_queue, &new_swap_chain, &desc]()
        {
            if (this->window && this->window_message_connection)
            {
#if UWP_APP
                winrt::Windows::UI::Xaml::Controls::SwapChainPanel swap_chain_panel = this->window->swap_chain_panel();
                this->use_xaml_composition = (swap_chain_panel != nullptr);

                if (this->use_xaml_composition)
                {
                    desc.Scaling = DXGI_SCALING_STRETCH;

                    Microsoft::WRL::ComPtr<ISwapChainPanelNative> native_panel;
                    if (FAILED(swap_chain_panel.as(IID_PPV_ARGS(&native_panel))) ||
                        FAILED(factory->CreateSwapChainForComposition(command_queue.Get(), &desc, nullptr, &new_swap_chain)) ||
                        FAILED(native_panel->SetSwapChain(new_swap_chain.Get())))
                    {
                        debug_fail();
                    }
                }
                else if (FAILED(factory->CreateSwapChainForCoreWindow(command_queue.Get(), this->window->handle().as<IUnknown>().get(), &desc, nullptr, &new_swap_chain)))
                {
                    debug_fail();
                }
#else
                if (FAILED(factory->CreateSwapChainForHwnd(command_queue.Get(), *this->window, &desc, nullptr, nullptr, &new_swap_chain)) ||
                    FAILED(factory->MakeWindowAssociation(*this->window, DXGI_MWA_NO_WINDOW_CHANGES)))
                {
                    debug_fail();
                }
#endif
            }
        });

        if (!new_swap_chain || FAILED(new_swap_chain.As(&this->swap_chain)))
        {
            ff::dx12::device_fatal_error("Swap chain creation failed");
            return false;
        }
    }
    else
    {
        ff::log::write(ff::log::type::dx12_target, "- Size didn't change");
    }

    if (frame_latency && FAILED(this->swap_chain->SetMaximumFrameLatency(static_cast<UINT>(frame_latency))))
    {
        ff::dx12::device_fatal_error("Swap chain failed to set frame latency");
        return false;
    }

#if UWP_APP
    // Scale the back buffer to the panel
    {
        DXGI_MATRIX_3X2_F inverse_scale{};
        inverse_scale._11 = inverse_scale._22 = 1 / static_cast<float>(size.dpi_scale);

        if (this->use_xaml_composition && FAILED(this->swap_chain->SetMatrixTransform(&inverse_scale)))
        {
            ff::dx12::device_fatal_error("Swap chain set matrix transform failed");
            return false;
        }
    }
#endif

    if (FAILED(this->swap_chain->SetRotation(ff::dxgi::get_dxgi_rotation(size.rotation, true))))
    {
        ff::dx12::device_fatal_error("Swap chain set rotation failed");
        return false;
    }

    this->back_buffer_index = static_cast<UINT>(this->swap_chain->GetCurrentBackBufferIndex());
    this->frame_latency_handle = ff::win_handle(frame_latency ? this->swap_chain->GetFrameLatencyWaitableObject() : nullptr);
    assert(this->target_textures.empty());

    for (size_t i = 0; i < buffer_count; i++)
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        if (FAILED(this->swap_chain->GetBuffer(static_cast<UINT>(i), IID_PPV_ARGS(&resource))))
        {
            ff::dx12::device_fatal_error("Swap chain get buffer failed");
            return false;
        }

        auto texture_resource = std::make_unique<ff::dx12::resource>(ff::string::concat("Swap chain back buffer ", i), resource.Get());
        this->target_textures.push_back(std::move(texture_resource));
        ff::dx12::create_target_view(this->target_textures.back().get(), this->target_views.cpu_handle(i));
    }

    this->size_changed_.notify(size);

    return true;
}

ff::signal_sink<ff::window_size>& ff::dx12::target_window::size_changed()
{
    return this->size_changed_;
}

size_t ff::dx12::target_window::buffer_count() const
{
    DXGI_SWAP_CHAIN_DESC desc;
    if (this->swap_chain && SUCCEEDED(this->swap_chain->GetDesc(&desc)))
    {
        return static_cast<size_t>(desc.BufferCount);
    }

    return 0;
}

void ff::dx12::target_window::buffer_count(size_t value)
{
    value = ::fix_buffer_count(value);
    if (this->buffer_count() != value)
    {
        ff::log::write(ff::log::type::dx12_target, "Set swap chain buffer count: ", value);

        const ff::window_size size = this->size();
        this->internal_size(size, value, ::fix_frame_latency(this->frame_latency(), value));
    }
}

size_t ff::dx12::target_window::frame_latency() const
{
    UINT value;
    DXGI_SWAP_CHAIN_DESC desc;

    if (this->swap_chain &&
        SUCCEEDED(this->swap_chain->GetDesc(&desc)) &&
        (desc.Flags & DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT) != 0 &&
        SUCCEEDED(this->swap_chain->GetMaximumFrameLatency(&value)))
    {
        return static_cast<size_t>(value);
    }

    return 0;
}

void ff::dx12::target_window::frame_latency(size_t value)
{
    value = ::fix_frame_latency(value, this->buffer_count());
    if (this->frame_latency() != value)
    {
        ff::log::write(ff::log::type::dx12_target, "Set swap chain frame latency: ", value);

        const ff::window_size size = this->size();
        this->internal_size(size, this->buffer_count(), value);
    }
}

bool ff::dx12::target_window::vsync() const
{
    return this->vsync_;
}

void ff::dx12::target_window::vsync(bool value)
{
    ff::log::write(ff::log::type::dx12_target, "Set swap chain vsync: ", value);
    this->vsync_ = value;
}

bool ff::dx12::target_window::allow_full_screen() const
{
    return this->allow_full_screen_;
}

bool ff::dx12::target_window::full_screen()
{
    if (this->allow_full_screen_ && *this)
    {
#if UWP_APP
        if (!this->cached_full_screen_uwp)
        {
            this->full_screen_uwp = this->window->application_view().IsFullScreenMode();
            this->cached_full_screen_uwp = true;
        }

        return this->full_screen_uwp;
#else
        BOOL full_screen = FALSE;
        Microsoft::WRL::ComPtr<IDXGIOutput> output;
        return SUCCEEDED(this->swap_chain->GetFullscreenState(&full_screen, &output)) && full_screen;
#endif
    }

    return this->was_full_screen_on_close;
}

bool ff::dx12::target_window::full_screen(bool value)
{
    if (this->allow_full_screen_ && *this && !value != !this->full_screen())
    {
        ff::log::write(ff::log::type::dx12_target, "Set swap chain full screen: ", value);

#if UWP_APP
        if (value)
        {
            return this->window->application_view().TryEnterFullScreenMode();
        }
        else
        {
            this->window->application_view().ExitFullScreenMode();
            return true;
        }
#else
        if (SUCCEEDED(this->swap_chain->SetFullscreenState(value, nullptr)))
        {
            return this->size(this->window->size());
        }
#endif
    }

    return false;
}

void ff::dx12::target_window::before_reset()
{
    this->frame_latency_handle.close();
    this->target_textures.clear();
}

bool ff::dx12::target_window::reset()
{
    return this->internal_reset(this->buffer_count(), this->frame_latency());
}

void ff::dx12::target_window::handle_message(ff::window_message& msg)
{
    switch (msg.msg)
    {
        case WM_SIZE:
        case WM_DISPLAYCHANGE:
        case WM_DPICHANGED:
            if (msg.msg != WM_SIZE || msg.wp != SIZE_MINIMIZED)
            {
                ff::dxgi_host().defer_resize(this, this->window->size());
            }
            break;

        case WM_CLOSE:
            if (this->allow_full_screen_)
            {
                ff::thread_dispatch::get_game()->send([this]()
                    {
                        this->was_full_screen_on_close = this->full_screen();
                    }, 500);
            }
            break;

        case WM_NCDESTROY:
            this->window_message_connection.disconnect();
            this->window = nullptr;
            break;

        case WM_SYSKEYDOWN:
            if (this->allow_full_screen_ && msg.wp == VK_RETURN) // ALT-ENTER to toggle full screen mode
            {
                ff::dxgi_host().defer_full_screen(!this->full_screen());
                msg.result = 0;
                msg.handled = true;
            }
            else if (this->main_window && msg.wp == VK_BACK)
            {
                if constexpr (ff::constants::profile_build)
                {
                    if (this->window->key_down(VK_SHIFT))
                    {
                        ff::thread_dispatch::get_game()->post([]()
                        {
                            ff::dx12::device_fatal_error("Pretend DX12 device fatal error for testing");
                        });
                    }
                    else
                    {
                        ff::dxgi_host().defer_reset_device(true);
                    }
                }
            }
            break;

        case WM_SYSCHAR:
            if (this->allow_full_screen_ && msg.wp == VK_RETURN)
            {
                // prevent a 'ding' sound when switching between modes
                msg.result = 0;
                msg.handled = true;
            }
            break;

#if !UWP_APP
        case WM_WINDOWPOSCHANGED:
            if (this->allow_full_screen_)
            {
                const WINDOWPOS& wp = *reinterpret_cast<const WINDOWPOS*>(msg.lp);
                if ((wp.flags & SWP_FRAMECHANGED) != 0 && !::IsIconic(msg.hwnd))
                {
                    ff::dxgi_host().defer_resize(this, this->window->size());
                }
            }
            break;
#endif
    }
}
