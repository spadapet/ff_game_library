#include "pch.h"
#include "graphics.h"
#include "target_window.h"
#include "texture_util.h"

ff::target_window::target_window()
    : target_window(ff::window::main())
{}

ff::target_window::target_window(ff::window* window)
    : window(window)
    , cached_size{}
    , main_window(ff::window::main() == window)
    , window_message_connection(window->message_sink().connect(std::bind(&target_window::handle_message, this, std::placeholders::_1)))
    , was_full_screen_on_close(false)
#if UWP_APP
    , use_xaml_composition(false)
    , cached_full_screen_uwp(false)
    , full_screen_uwp(false)
#endif
#if DXVER == 12
    , target_views(ff_dx::cpu_target_descriptors().alloc_range(ff::target_window::BACK_BUFFER_COUNT))
    , back_buffer_index(0)
#endif
{
    this->size(this->window->size());

    ff_dx::add_device_child(this, ff_dx::device_reset_priority::target_window);

    if (this->main_window)
    {
        ff::graphics::defer::set_full_screen_target(this);
    }
}

ff::target_window::~target_window()
{
    ff_dx::wait_for_idle();

    if (this->main_window && this->swap_chain)
    {
        this->swap_chain->SetFullscreenState(FALSE, nullptr);
    }

    ff::graphics::defer::remove_target(this);
    ff_dx::remove_device_child(this);
}

ff::target_window::operator bool() const
{
    return this->swap_chain && this->window;
}

DXGI_FORMAT ff::target_window::format() const
{
    return DXGI_FORMAT_B8G8R8A8_UNORM;
}

ff::window_size ff::target_window::size() const
{
    return this->cached_size;
}

#if DXVER == 11

ID3D11Texture2D* ff::target_window::dx11_target_texture()
{
    return this->texture_.Get();
}

ID3D11RenderTargetView* ff::target_window::dx11_target_view()
{
    return this->view_.Get();
}

void ff::target_window::clear(ff::dxgi::command_context_base& context, const DirectX::XMFLOAT4& clear_color)
{
    ff_dx::device_state::get(context).clear_target(this->dx11_target_view(), clear_color);
}

bool ff::target_window::pre_render(const DirectX::XMFLOAT4* clear_color)
{
    if (clear_color)
    {
        this->clear(ff::dx11::get_device_state(), *clear_color);
    }

    return true;
}

bool ff::target_window::present()
{
    ff_dx::get_device_state().set_targets(nullptr, 0, nullptr);

    HRESULT hr = *this ? this->swap_chain->Present(1, 0) : E_FAIL;
    if (hr != DXGI_ERROR_DEVICE_RESET && hr != DXGI_ERROR_DEVICE_REMOVED)
    {
        this->render_presented_.notify(this);
        return true;
    }

    return false;
}

void ff::target_window::before_resize()
{
    this->view_.Reset();
    this->texture_.Reset();
}

void ff::target_window::internal_reset()
{
    this->swap_chain.Reset();
    this->view_.Reset();
    this->texture_.Reset();

    ff_dx::get_device_state().clear();
}

#elif DXVER == 12

ff::dx12::resource& ff::target_window::dx12_target_texture()
{
    return *this->target_textures[this->back_buffer_index];
}

D3D12_CPU_DESCRIPTOR_HANDLE ff::target_window::dx12_target_view()
{
    return this->target_views.cpu_handle(this->back_buffer_index);
}

void ff::target_window::clear(ff::dxgi::command_context_base& context, const DirectX::XMFLOAT4& clear_color)
{
    ff::dx12::commands::get(context).clear(*this, clear_color);
}

bool ff::target_window::pre_render(const DirectX::XMFLOAT4* clear_color)
{
    if (*this)
    {
        ff::dx12::commands commands = ff::dx12::direct_queue().new_commands();

        if (clear_color)
        {
            this->clear(commands, *clear_color);
        }
        else
        {
            commands.discard(*this);
        }

        return true;
    }

    return false;
}

bool ff::target_window::present()
{
    if (*this)
    {
        // Transition
        {
            ff::dx12::commands commands = ff::dx12::direct_queue().new_commands();
            commands.resource_state(*this->target_textures[this->back_buffer_index], D3D12_RESOURCE_STATE_PRESENT);
        }

        HRESULT hr = this->swap_chain->Present(1, 0);

        if (hr != DXGI_ERROR_DEVICE_RESET && hr != DXGI_ERROR_DEVICE_REMOVED)
        {
            this->back_buffer_index = static_cast<size_t>(this->swap_chain->GetCurrentBackBufferIndex());
            this->render_presented_.notify(this);

            return true;
        }
    }

    return false;
}

void ff::target_window::before_resize()
{
    ff_dx::wait_for_idle();

    for (size_t i = 0; i < ff::target_window::BACK_BUFFER_COUNT; i++)
    {
        this->target_textures[i].reset();
    }
}

void ff::target_window::internal_reset()
{
    this->before_resize();
    this->swap_chain.Reset();
}

#endif

ff::signal_sink<ff::dxgi::target_base*>& ff::target_window::render_presented()
{
    return this->render_presented_;
}

ff::dxgi::target_access_base& ff::target_window::target_access()
{
    return *this;
}

size_t ff::target_window::target_array_start() const
{
    return 0;
}

size_t ff::target_window::target_array_size() const
{
    return 1;
}

size_t ff::target_window::target_mip_start() const
{
    return 0;
}

size_t ff::target_window::target_mip_size() const
{
    return 1;
}

bool ff::target_window::size(const ff::window_size& size)
{
    ff::window_size old_size = this->cached_size;
    ff::point_t<UINT> buffer_size = size.rotated_pixel_size().cast<UINT>();
    this->cached_size = size;
#if UWP_APP
    this->cached_full_screen_uwp = false;
#endif

    if (this->swap_chain && old_size != size) // on game thread
    {
        this->before_resize();

        DXGI_SWAP_CHAIN_DESC1 desc;
        this->swap_chain->GetDesc1(&desc);
        if (FAILED(this->swap_chain->ResizeBuffers(0, buffer_size.x, buffer_size.y, desc.Format, desc.Flags)))
        {
            assert(false);
            return false;
        }
    }
    else if (!this->swap_chain) // first init on UI thread, reset is on game thread
    {
        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Width = buffer_size.x;
        desc.Height = buffer_size.y;
        desc.Format = this->format();
        desc.SampleDesc.Count = 1;
        desc.BufferCount = ff::target_window::BACK_BUFFER_COUNT;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.Scaling = DXGI_SCALING_STRETCH;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
#if DXVER == 12
        desc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT; // sets latency to 1 frame
#endif

        Microsoft::WRL::ComPtr<IDXGISwapChain1> new_swap_chain;
#if DXVER == 11
        Microsoft::WRL::ComPtr<ID3D11DeviceX> device = ff_dx::device();
        Microsoft::WRL::ComPtr<IDXGIDeviceX> dxgi_device;
        Microsoft::WRL::ComPtr<IDXGIAdapterX> adapter;
        Microsoft::WRL::ComPtr<IDXGIFactoryX> factory;

        if (FAILED(device.As(&dxgi_device)) || FAILED(dxgi_device->GetParent(IID_PPV_ARGS(&adapter))) || FAILED(adapter->GetParent(IID_PPV_ARGS(&factory))))
        {
            assert(false);
            return false;
        }

#elif DXVER == 12
        Microsoft::WRL::ComPtr<IDXGIFactoryX> factory = ff::dx12::factory();
        Microsoft::WRL::ComPtr<ID3D12CommandQueueX> device = ff::dx12::get_command_queue(ff::dx12::direct_queue());
#endif

        ff::thread_dispatch::get_main()->send([this, factory, device, &new_swap_chain, &desc]()
            {
#if UWP_APP
                if (this->window)
                {
                    Windows::UI::Xaml::Controls::SwapChainPanel^ swap_chain_panel = this->window->swap_chain_panel();
                    this->use_xaml_composition = (swap_chain_panel != nullptr);

                    if (this->use_xaml_composition)
                    {
                        Microsoft::WRL::ComPtr<ISwapChainPanelNative> native_panel;

                        if (FAILED(reinterpret_cast<IUnknown*>(swap_chain_panel)->QueryInterface(IID_PPV_ARGS(&native_panel))) ||
                            FAILED(factory->CreateSwapChainForComposition(device.Get(), &desc, nullptr, &new_swap_chain)) ||
                            FAILED(native_panel->SetSwapChain(new_swap_chain.Get())))
                        {
                            assert(false);
                        }
                    }
                    else if (FAILED(factory->CreateSwapChainForCoreWindow(device.Get(), reinterpret_cast<IUnknown*>(this->window->handle()), &desc, nullptr, &new_swap_chain)))
                    {
                        assert(false);
                    }
                }
#else
                if (!*this->window ||
                    FAILED(factory->CreateSwapChainForHwnd(device.Get(), *this->window, &desc, nullptr, nullptr, &new_swap_chain)) ||
                    FAILED(factory->MakeWindowAssociation(*this->window, DXGI_MWA_NO_WINDOW_CHANGES)))
                {
                    assert(false);
                }
#endif
            });

        if (!new_swap_chain || FAILED(new_swap_chain.As(&this->swap_chain)))
        {
            assert(false);
            return false;
        }
    }

    DXGI_MODE_ROTATION display_rotation = ff::dxgi::get_display_rotation(
        ff::dxgi::get_dxgi_rotation(size.native_rotation),
        ff::dxgi::get_dxgi_rotation(size.current_rotation));

#if UWP_APP
    // Scale the back buffer to the panel
    DXGI_MATRIX_3X2_F inverse_scale{};
    inverse_scale._11 = inverse_scale._22 = 1 / static_cast<float>(size.dpi_scale);
#endif

    if (!this->swap_chain ||
#if UWP_APP
        (this->use_xaml_composition && FAILED(this->swap_chain->SetMatrixTransform(&inverse_scale))) ||
#endif
        FAILED(this->swap_chain->SetRotation(display_rotation)))
    {
        assert(false);
        return false;
    }

#if DXVER == 11
    if ((!this->texture_ && FAILED(this->swap_chain->GetBuffer(0, IID_PPV_ARGS(&this->texture_)))) ||
        (!this->view_ && (this->view_ = ff::dx11::create_target_view(this->texture_.Get())) == nullptr))
    {
        assert(false);
        return false;
    }
#elif DXVER == 12
    this->back_buffer_index = static_cast<UINT>(this->swap_chain->GetCurrentBackBufferIndex());

    for (size_t i = 0; i < ff::target_window::BACK_BUFFER_COUNT; i++)
    {
        if (!this->target_textures[i])
        {
            Microsoft::WRL::ComPtr<ID3D12ResourceX> resource;
            if (FAILED(this->swap_chain->GetBuffer(static_cast<UINT>(i), IID_PPV_ARGS(&resource))))
            {
                assert(false);
                return false;
            }

            this->target_textures[i] = std::make_unique<ff::dx12::resource>(resource.Get());
        }

        ff::dx12::create_target_view(this->target_textures[i].get(), this->target_views.cpu_handle(i));
    }
#endif

    this->size_changed_.notify(size);

    return true;
}

ff::signal_sink<ff::window_size>& ff::target_window::size_changed()
{
    return this->size_changed_;
}

bool ff::target_window::allow_full_screen() const
{
    return this->main_window;
}

bool ff::target_window::full_screen()
{
    if (this->main_window && *this)
    {
#if UWP_APP
        if (!this->cached_full_screen_uwp)
        {
            this->full_screen_uwp = this->window->application_view()->IsFullScreenMode;
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

bool ff::target_window::full_screen(bool value)
{
    if (this->main_window && *this && !value != !this->full_screen())
    {
#if UWP_APP
        if (value)
        {
            return this->window->application_view()->TryEnterFullScreenMode();
        }
        else
        {
            this->window->application_view()->ExitFullScreenMode();
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

bool ff::target_window::reset()
{
    BOOL full_screen = FALSE;
    if (this->main_window && this->swap_chain)
    {
        Microsoft::WRL::ComPtr<IDXGIOutput> output;
        this->swap_chain->GetFullscreenState(&full_screen, &output);
        this->swap_chain->SetFullscreenState(FALSE, nullptr);
    }

    this->internal_reset();

    if (!this->window || !this->size(this->window->size()))
    {
        assert(false);
        return false;
    }

    if (this->main_window && this->swap_chain && full_screen)
    {
        this->swap_chain->SetFullscreenState(TRUE, nullptr);
    }

    return *this;
}

void ff::target_window::handle_message(ff::window_message& msg)
{
    switch (msg.msg)
    {
        case WM_ACTIVATE:
            if (LOWORD(msg.wp) == WA_INACTIVE && this->main_window)
            {
                ff::graphics::defer::full_screen(false);
            }
            break;

        case WM_SIZE:
            if (msg.wp != SIZE_MINIMIZED)
            {
                ff::graphics::defer::resize_target(this, this->window->size());
            }
            break;

        case WM_DESTROY:
            this->window_message_connection.disconnect();

            if (this->main_window)
            {
                ff::thread_dispatch::get_game()->send([this]()
                    {
                        this->was_full_screen_on_close = this->full_screen();
                        this->full_screen(false);
                    });
            }

            this->window = nullptr;
            break;

        case WM_SYSKEYDOWN:
            if (this->main_window && msg.wp == VK_RETURN) // ALT-ENTER to toggle full screen mode
            {
                ff::graphics::defer::full_screen(!this->full_screen());
                msg.result = 0;
                msg.handled = true;
            }
            else if (this->main_window && msg.wp == VK_BACK)
            {
#ifdef _DEBUG
                ff::graphics::defer::validate_device(true);
#endif
            }
            break;

        case WM_SYSCHAR:
            if (this->main_window && msg.wp == VK_RETURN)
            {
                // prevent a 'ding' sound when switching between modes
                msg.result = 0;
                msg.handled = true;
            }
            break;

#if !UWP_APP
        case WM_WINDOWPOSCHANGED:
            if (this->main_window)
            {
                const WINDOWPOS& wp = *reinterpret_cast<const WINDOWPOS*>(msg.lp);
                if ((wp.flags & SWP_FRAMECHANGED) != 0 && !::IsIconic(msg.hwnd))
                {
                    ff::graphics::defer::resize_target(this, this->window->size());
                }
            }
            break;
#endif
    }
}
