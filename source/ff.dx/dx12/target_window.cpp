#include "pch.h"
#include "dx12/commands.h"
#include "dx12/descriptor_allocator.h"
#include "dx12/device_reset_priority.h"
#include "dx12/globals.h"
#include "dx12/resource.h"
#include "dx12/target_window.h"
#include "dx12/queue.h"
#include "dxgi/interop.h"
#include "types/color.h"
#include "types/operators.h"

constexpr size_t MIN_BUFFER_COUNT = 2;
constexpr size_t MAX_BUFFER_COUNT = 4;

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

static DXGI_MODE_ROTATION get_dxgi_rotation(int dmod, bool ccw)
{
    switch (dmod)
    {
        default:
            return DXGI_MODE_ROTATION_IDENTITY;

        case DMDO_90:
            return ccw ? DXGI_MODE_ROTATION_ROTATE270 : DXGI_MODE_ROTATION_ROTATE90;

        case DMDO_180:
            return DXGI_MODE_ROTATION_ROTATE180;

        case DMDO_270:
            return ccw ? DXGI_MODE_ROTATION_ROTATE90 : DXGI_MODE_ROTATION_ROTATE270;
    }
}

static ff::rect_int convert_rect(const RECT& rect)
{
    return ff::rect_int(rect.left, rect.top, rect.right, rect.bottom);
}

static bool full_screen_style(HWND hwnd)
{
    LONG style = ::GetWindowLong(hwnd, GWL_STYLE);
    return (style & WS_POPUP) != 0;
}

static bool update_window_styles(HWND hwnd, bool full_screen, const ff::rect_int* windowed_rect = nullptr)
{
    HMONITOR monitor = ::MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitor_info{ sizeof(monitor_info) };
    assert_ret_val(monitor && ::GetMonitorInfo(monitor, &monitor_info), false);

    RECT temp_rect;
    const ff::rect_int old_window_rect = ::GetWindowRect(hwnd, &temp_rect) ? ::convert_rect(temp_rect) : ff::rect_int{};
    const ff::rect_int monitor_rect = ::convert_rect(full_screen ? monitor_info.rcMonitor : monitor_info.rcWork);
    ff::rect_int new_window_rect = (full_screen ? monitor_rect : (windowed_rect ? *windowed_rect : old_window_rect));

    if (!full_screen && windowed_rect)
    {
        new_window_rect = new_window_rect.move_inside(monitor_rect).crop(monitor_rect);
    }

    const LONG old_style = ::GetWindowLong(hwnd, GWL_STYLE);
    const LONG new_style = (full_screen ? WS_POPUP : WS_OVERLAPPEDWINDOW) | (old_style & WS_VISIBLE);
    const LONG relevant_styles = WS_POPUP | WS_OVERLAPPEDWINDOW | WS_VISIBLE;
    const bool style_changed = (old_style & relevant_styles) != (new_style & relevant_styles);

    if (style_changed || old_window_rect != new_window_rect)
    {
        if (style_changed)
        {
            ::SetWindowLong(hwnd, GWL_STYLE, new_style);
        }

        ::SetWindowPos(hwnd, nullptr,
            new_window_rect.left, new_window_rect.top, new_window_rect.width(), new_window_rect.height(),
            SWP_FRAMECHANGED | SWP_SHOWWINDOW);

        return true;
    }

    return false;
}

ff::dx12::target_window::target_window(ff::window* window, const ff::dxgi::target_window_params& params)
    : window(window)
    , window_message_connection(window->message_sink().connect(std::bind(&target_window::handle_message, this, std::placeholders::_1)))
    , params(params)
    , target_views(ff::dx12::cpu_target_descriptors().alloc_range(MAX_BUFFER_COUNT))
{
    this->params.buffer_count = ::fix_buffer_count(this->params.buffer_count);
    this->params.frame_latency = ::fix_frame_latency(this->params.frame_latency, this->params.buffer_count);

    RECT windowed_rect;
    if (::GetWindowRect(*this->window, &windowed_rect))
    {
        this->windowed_rect = ::convert_rect(windowed_rect);
    }

    assert_msg(!::full_screen_style(*this->window), "Full screen must be set by target_window, not during creation");
    this->size(this->window->size());

    ff::dx12::add_device_child(this, ff::dx12::device_reset_priority::target_window);
}

ff::dx12::target_window::~target_window()
{
    ff::dxgi::wait_for_idle();
    ff::dxgi::remove_target(this);
    ff::dx12::remove_device_child(this);
}

ff::dx12::target_window::operator bool() const
{
    return this->swap_chain &&
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
    if (this->params.extra_render_target)
    {
        return ff::dx12::target_access::get(this->extra_render_target()).dx12_target_texture();
    }

    return *this->target_textures[this->back_buffer_index];
}

D3D12_CPU_DESCRIPTOR_HANDLE ff::dx12::target_window::dx12_target_view()
{
    if (this->params.extra_render_target)
    {
        return ff::dx12::target_access::get(this->extra_render_target()).dx12_target_view();
    }

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
        ff::dxgi::target_base& target = this->params.extra_render_target ? this->extra_render_target() : *this;
        if (clear_color)
        {
            assert_msg(*clear_color == ff::color_black(), "Swap chain render targets must be cleared to black");
            target.clear(context, *clear_color);
        }
        else
        {
            ff::dx12::commands::get(context).discard(target);
        }

        return true;
    }

    return false;
}

bool ff::dx12::target_window::end_render(ff::dxgi::command_context_base& context)
{
    if (*this && ff::dx12::device_valid())
    {
        std::unique_ptr<ff::dx12::commands> new_commands;

        ff::dx12::commands& commands = ff::dx12::commands::get(context);
        ff::dx12::queue& queue = commands.queue();
        ff::dx12::commands* present_commands = &commands;
        ff::dx12::resource& back_buffer_resource = *this->target_textures[this->back_buffer_index];

        if (this->params.extra_render_target)
        {
            // Finish all rendering to extra render target
            queue.execute(commands);

            // Copy extra render target to back buffer
            ff::dxgi::target_base& extra_target = this->extra_render_target();
            ff::dx12::resource& extra_resource = ff::dx12::target_access::get(extra_target).dx12_target_texture();
            new_commands = queue.new_commands();
            present_commands = new_commands.get();
            present_commands->copy_resource(back_buffer_resource, extra_resource);
        }

        if (this->frame_latency_handle)
        {
            ff::perf_timer timer(::perf_render_wait);
            this->frame_latency_handle.wait(INFINITE, false);
        }

        present_commands->resource_state(back_buffer_resource, D3D12_RESOURCE_STATE_PRESENT);
        queue.execute(*present_commands);

        ff::perf_timer timer(::perf_render_present);
        HRESULT hr = this->swap_chain->Present(this->vsync() ? 1 : 0, 0);
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

bool ff::dx12::target_window::internal_reset()
{
    assert_ret_val(this->window, false);

    this->before_resize();
    this->swap_chain.Reset();

    bool has_window = false;
    ff::window_size size;
    ff::thread_dispatch::get_main()->send([this, &has_window, &size]()
    {
        if (this->window)
        {
            has_window = true;
            size = this->window->size();
        }
    });

    assert_ret_val(has_window && this->size(size), false);

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

bool ff::dx12::target_window::size(const ff::window_size& input_size)
{
    assert_ret_val(this->window, false);

    if (this->swap_chain && (this->frame_latency() == 0) != (this->params.frame_latency == 0))
    {
        // On game thread: Turning frame latency on/off requires recreating everything
        if (!this->internal_reset())
        {
            ff::dx12::device_fatal_error("Swap chain failed to change frame latency");
            return false;
        }

        return true;
    }

    // Fix up the input size, maybe the window has zero client area size
    ff::window_size size = input_size;
    size.logical_pixel_size.x = std::max<size_t>(size.logical_pixel_size.x, 1);
    size.logical_pixel_size.y = std::max<size_t>(size.logical_pixel_size.y, 1);
    size.dpi_scale = std::min<double>(size.dpi_scale, 1.0);

    ff::log::write(ff::log::type::dx12_target,
        "Swap chain set size.",
        " Size=", size.logical_pixel_size.x, ",", size.logical_pixel_size.y,
        " Rotate=", size.rotated_degrees(),
        " Buffers=", this->params.buffer_count,
        " Latency=", this->params.frame_latency,
        " VSync=", this->vsync());

    ff::window_size old_size = this->cached_size;
    ff::point_t<UINT> buffer_size = size.physical_pixel_size().cast<UINT>();
    this->cached_size = size;
    this->before_resize();

    if (this->swap_chain && (old_size != size || this->buffer_count() != this->params.buffer_count)) // on game thread
    {
        ff::log::write(ff::log::type::dx12_target, "- Resize existing buffers");

        DXGI_SWAP_CHAIN_DESC1 desc;
        if (FAILED(this->swap_chain->GetDesc1(&desc)) || FAILED(this->swap_chain->ResizeBuffers(
            static_cast<UINT>(this->params.buffer_count), buffer_size.x, buffer_size.y, desc.Format, desc.Flags)))
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
        desc.BufferCount = static_cast<UINT>(this->params.buffer_count);
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.Scaling = DXGI_SCALING_NONE;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        desc.Flags = this->params.frame_latency ? DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT : 0;

        Microsoft::WRL::ComPtr<IDXGISwapChain1> new_swap_chain;
        Microsoft::WRL::ComPtr<IDXGIFactory2> factory = ff::dx12::factory();
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> command_queue = ff::dx12::get_command_queue(ff::dx12::direct_queue());

        ff::thread_dispatch::get_main()->send([this, factory, command_queue, &new_swap_chain, &desc]()
        {
            if (!this->window ||
                FAILED(factory->CreateSwapChainForHwnd(command_queue.Get(), *this->window, &desc, nullptr, nullptr, &new_swap_chain)) ||
                FAILED(factory->MakeWindowAssociation(*this->window, DXGI_MWA_NO_WINDOW_CHANGES)))
            {
                debug_fail();
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

    if (this->params.frame_latency && FAILED(this->swap_chain->SetMaximumFrameLatency(static_cast<UINT>(this->params.frame_latency))))
    {
        ff::dx12::device_fatal_error("Swap chain failed to set frame latency");
        return false;
    }

    if (FAILED(this->swap_chain->SetRotation(::get_dxgi_rotation(size.rotation, true))))
    {
        ff::dx12::device_fatal_error("Swap chain set rotation failed");
        return false;
    }

    this->back_buffer_index = static_cast<UINT>(this->swap_chain->GetCurrentBackBufferIndex());
    this->frame_latency_handle = ff::win_handle(this->params.frame_latency ? this->swap_chain->GetFrameLatencyWaitableObject() : nullptr);
    assert(this->target_textures.empty() && this->extra_render_targets.empty());

    for (size_t i = 0; i < this->params.buffer_count; i++)
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        if (FAILED(this->swap_chain->GetBuffer(static_cast<UINT>(i), IID_PPV_ARGS(&resource))))
        {
            ff::dx12::device_fatal_error("Swap chain get buffer failed");
            return false;
        }

        auto texture_resource = std::make_unique<ff::dx12::resource>(ff::string::concat("Swap chain back buffer ", i), resource.Get());
        this->target_textures.push_back(std::move(texture_resource));
        this->target_textures.back()->create_target_view(this->target_views.cpu_handle(i));

        if (this->params.extra_render_target)
        {
            auto texture = ff::dxgi::create_render_texture(size.physical_pixel_size(), this->format(), 1, 1, 1, &ff::color_black());
            this->extra_render_targets.push_back(ff::dxgi::create_target_for_texture(texture, 0, 0, 0, size.rotation, size.dpi_scale));
        }
    }

    this->size_changed_.notify(size);

    return true;
}

ff::dxgi::target_base& ff::dx12::target_window::extra_render_target()
{
    assert(this->params.extra_render_target);
    return *this->extra_render_targets[this->back_buffer_index];
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
    this->params.buffer_count = ::fix_buffer_count(value);
    this->params.frame_latency = ::fix_frame_latency(this->params.frame_latency, this->params.buffer_count);

    if (this->buffer_count() != value || this->frame_latency() != this->params.frame_latency)
    {
        ff::log::write(ff::log::type::dx12_target, "Set swap chain buffer count: ", value);
        this->size(this->size());
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
    this->params.frame_latency = ::fix_frame_latency(value, this->buffer_count());
    if (this->frame_latency() != this->params.frame_latency)
    {
        ff::log::write(ff::log::type::dx12_target, "Set swap chain frame latency: ", this->params.frame_latency);
        this->size(this->size());
    }
}

bool ff::dx12::target_window::vsync() const
{
    return this->params.vsync;
}

void ff::dx12::target_window::vsync(bool value)
{
    ff::log::write(ff::log::type::dx12_target, "Set swap chain vsync: ", value);
    this->params.vsync = value;
}

bool ff::dx12::target_window::allow_full_screen() const
{
    return this->params.allow_full_screen;
}

// Called on game or main thread
bool ff::dx12::target_window::full_screen()
{
    if (this->allow_full_screen())
    {
        std::scoped_lock lock(this->window_mutex);
        return this->window ? ::full_screen_style(*this->window) : this->was_full_screen_on_close;
    }

    return false;
}

bool ff::dx12::target_window::full_screen(bool value)
{
    bool status = false;

    if (this->allow_full_screen())
    {
        ff::log::write(ff::log::type::dx12_target, "Set full screen: ", value);

        std::scoped_lock lock(this->window_mutex);
        if (this->window)
        {
            status = true;
            this->window->dispatch()->post([hwnd = this->window->handle(), windowed_rect = this->windowed_rect, value]()
                {
                    ::update_window_styles(hwnd, value, &windowed_rect);
                });
        }
    }

    return status;
}

void ff::dx12::target_window::before_reset()
{
    this->frame_latency_handle.close();
    this->target_textures.clear();
    this->extra_render_targets.clear();
}

bool ff::dx12::target_window::reset()
{
    return this->internal_reset();
}

void ff::dx12::target_window::handle_message(ff::window_message& msg)
{
    switch (msg.msg)
    {
        case WM_ENTERSIZEMOVE:
            this->resizing_data = { true };
            break;

        case WM_EXITSIZEMOVE:
            if (this->resizing_data.resizing)
            {
                if (this->resizing_data.size_valid)
                {
                    ff::dxgi::defer_resize_target(this, this->resizing_data.size);
                }

                this->resizing_data = {};
            }
            break;

        case WM_SIZE:
            if (msg.wp != SIZE_MINIMIZED)
            {
                if (::full_screen_style(msg.hwnd))
                {
                    ::update_window_styles(msg.hwnd, true);
                }

                ff::window_size size = this->window->size();
                if (this->resizing_data.resizing)
                {
                    this->resizing_data.size_valid = true;
                    this->resizing_data.size = size;
                }
                else
                {
                    ff::dxgi::defer_resize_target(this, size);
                }
            }
            break;

        case WM_DISPLAYCHANGE:
            if (::full_screen_style(msg.hwnd))
            {
                ::update_window_styles(msg.hwnd, true);
            }
            break;

        case WM_MOVE:
            {
                RECT windowed_rect{};
                if (!::full_screen_style(msg.hwnd) && ::GetWindowRect(msg.hwnd, &windowed_rect))
                {
                    this->windowed_rect = ::convert_rect(windowed_rect);
                }
            }
            break;

        case WM_GETMINMAXINFO:
            {
                RECT rect{ 0, 0, 240, 135 };
                const DWORD style = static_cast<DWORD>(::GetWindowLong(msg.hwnd, GWL_STYLE));
                const DWORD exStyle = static_cast<DWORD>(::GetWindowLong(msg.hwnd, GWL_EXSTYLE));
                if (::AdjustWindowRectExForDpi(&rect, style, FALSE, exStyle, ::GetDpiForWindow(msg.hwnd)))
                {
                    MINMAXINFO& mm = *reinterpret_cast<MINMAXINFO*>(msg.lp);
                    mm.ptMinTrackSize.x = rect.right - rect.left;
                    mm.ptMinTrackSize.y = rect.bottom - rect.top;
                }
            }
            break;

        case WM_DESTROY:
            {
                std::scoped_lock lock(this->window_mutex);
                this->was_full_screen_on_close = ::full_screen_style(msg.hwnd);
                this->window_message_connection.disconnect();
                this->window = nullptr;
            }
            break;

        case WM_KEYDOWN:
            if (msg.wp == VK_F11 && !(msg.lp & 0x40000000)) // wasn't already down
            {
                ff::dxgi::defer_full_screen(this, !::full_screen_style(msg.hwnd));
            }
            break;

        case WM_SYSKEYDOWN:
            if (this->allow_full_screen() && msg.wp == VK_RETURN) // ALT-ENTER to toggle full screen mode
            {
                ff::dxgi::defer_full_screen(this, !::full_screen_style(msg.hwnd));
                msg.result = 0;
                msg.handled = true;
            }
            else if (msg.wp == VK_BACK)
            {
                if constexpr (ff::constants::profile_build)
                {
                    if (::GetKeyState(VK_SHIFT) < 0)
                    {
                        ff::thread_dispatch::get_game()->post([]()
                        {
                            ff::dx12::device_fatal_error("Pretend DX12 device fatal error for testing");
                        });
                    }
                    else
                    {
                        ff::dxgi::defer_reset_device(true);
                    }
                }
            }
            break;

        case WM_SYSCHAR:
            if (this->allow_full_screen() && msg.wp == VK_RETURN)
            {
                // prevent a 'ding' sound when switching between modes
                msg.result = 0;
                msg.handled = true;
            }
            break;
    }
}
