#include "pch.h"
#include "graphics/dx12/commands.h"
#include "graphics/dx12/descriptor_allocator.h"
#include "graphics/dx12/device_reset_priority.h"
#include "graphics/dx12/dx12_globals.h"
#include "graphics/dx12/resource.h"
#include "graphics/dx12/target_window.h"
#include "graphics/dx12/queue.h"
#include "graphics/dxgi/dxgi_globals.h"
#include "graphics/types/color.h"
#include "graphics/types/operators.h"

static ff::perf_counter perf_render_wait("Wait", ff::perf_color::cyan, ff::perf_chart_t::render_wait);
static ff::perf_counter perf_render_present("Present", ff::perf_color::cyan, ff::perf_chart_t::render_wait);

std::array<ff::dx12::target_window::pacing_stage, 4> ff::dx12::target_window::pacing_stages =
{
    // latency, vsync
    ff::dx12::target_window::pacing_stage{ 1, true },
    ff::dx12::target_window::pacing_stage{ 1, false },
    ff::dx12::target_window::pacing_stage{ 2, true },
    ff::dx12::target_window::pacing_stage{ 2, false },
};

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

ff::dx12::target_window::target_window(ff::window* window)
    : window(window)
    , target_views(ff::dx12::cpu_target_descriptors().alloc_range(ff::dx12::target_window::BUFFER_COUNT))
{
    this->size(this->size());

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
    return this->swap_chain && this->target_textures.size() && ff::dx12::device_valid();
}

DXGI_FORMAT ff::dx12::target_window::format() const
{
    return DXGI_FORMAT_B8G8R8A8_UNORM;
}

ff::window_size ff::dx12::target_window::size() const
{
    return (ff::thread_dispatch::get_main()->current_thread() && this->window)
        ? this->window->size()
        : this->cached_size;
}

ff::dx12::resource& ff::dx12::target_window::dx12_target_texture()
{
    return this->target_textures[this->swap_chain->GetCurrentBackBufferIndex()];
}

D3D12_CPU_DESCRIPTOR_HANDLE ff::dx12::target_window::dx12_target_view()
{
    return this->target_views.cpu_handle(this->swap_chain->GetCurrentBackBufferIndex());
}

void ff::dx12::target_window::clear(ff::dxgi::command_context_base& context, const ff::color& clear_color)
{
    ff::dx12::commands::get(context).clear(*this, clear_color);
}

void ff::dx12::target_window::discard(ff::dxgi::command_context_base& context)
{
    ff::dx12::commands::get(context).discard(*this);
}

bool ff::dx12::target_window::begin_render(ff::dxgi::command_context_base& context, const ff::color* clear_color)
{
    check_ret_val(*this, false);

    if (clear_color)
    {
        assert_msg(*clear_color == ff::color_black(), "Swap chain render targets must be cleared to black");
        this->clear(context, *clear_color);
    }
    else
    {
        this->discard(context);
    }

    return true;
}

bool ff::dx12::target_window::end_render(ff::dxgi::command_context_base& context)
{
    check_ret_val(*this, false);

    if (this->latency_handle)
    {
        ff::perf_timer perf_timer(::perf_render_wait);
        this->latency_handle.wait(INFINITE, false);
    }

    ff::dx12::commands& commands = ff::dx12::commands::get(context);
    commands.resource_state(this->dx12_target_texture(), D3D12_RESOURCE_STATE_PRESENT);
    commands.queue().execute(commands);

    ff::perf_timer timer(::perf_render_present);
    HRESULT hr = this->swap_chain->Present(this->pacing_vsync() ? 1 : 0, 0);
    this->update_pacing();

    return hr != DXGI_ERROR_DEVICE_RESET && hr != DXGI_ERROR_DEVICE_REMOVED;
}

void ff::dx12::target_window::update_pacing()
{
    // EMA = exponential moving average
    constexpr size_t ema_window = 16;
    constexpr double ema_alpha = 1.0 / ema_window;
    constexpr double good_fps = 58.0;
    constexpr double bad_fps = 54.0;
    constexpr size_t window_count_to_improve = 2;
    constexpr size_t window_count_ignore_after_resize = 1;

    double frame_time = this->pacing.timer.tick();
    check_ret(!this->resizing_data.resizing);

    this->pacing.average = this->pacing.average * (1.0 - ema_alpha) + frame_time * ema_alpha;

    // Don't check the average every frame, just after a "window" of 16 frames
    check_ret(!(++this->pacing.count % ema_window));

    size_t window_count = this->pacing.count / ema_window;
    uint32_t before_latency = this->pacing_latency();
    size_t before_stage = this->pacing.stage;

    if (this->pacing.average <= (1.0 / good_fps) && this->pacing.stage > 0 && window_count >= window_count_to_improve)
    {
        this->pacing.stage--;
    }
    else if (this->pacing.average >= (1.0 / bad_fps) && (this->pacing.stage > 0 || window_count > window_count_ignore_after_resize))
    {
        this->pacing.stage = std::min(this->pacing.stage + 1, pacing_stages.size() - 1);
    }

    if (before_latency != this->pacing_latency())
    {
        this->latency_handle.close();

        if (SUCCEEDED(this->swap_chain->SetMaximumFrameLatency(this->pacing_latency())))
        {
            this->latency_handle = ff::win_handle(this->swap_chain->GetFrameLatencyWaitableObject());
        }
    }

    if (before_stage != this->pacing.stage)
    {
        ff::log::write(ff::log::type::dx12_target,
            "Frame pacing ", (before_stage < this->pacing.stage ? "FAILS" : "IMPROVES"), ". New stage:", this->pacing.stage,
            ", average frame time:", this->pacing.average,
            ", latency:", this->pacing_latency(),
            ", vsync:", this->pacing_vsync());
    }
}

void ff::dx12::target_window::before_resize()
{
    ff::dx12::wait_for_idle();
    this->before_reset();
}

bool ff::dx12::target_window::internal_reset()
{
    this->before_resize();
    this->swap_chain.Reset();

    bool has_window = false;
    ff::window_size size{};
    ff::thread_dispatch::get_main()->send([this, &has_window, &size]()
    {
        if (this->window)
        {
            has_window = true;
            size = this->window->size();
        }
    });

    if (has_window)
    {
        assert_ret_val(this->size(size), false);
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

bool ff::dx12::target_window::size(const ff::window_size& input_size)
{
    // Fix up the input size, maybe the window has zero client area size
    ff::window_size size = input_size;
    size.logical_pixel_size.x = std::max<size_t>(size.logical_pixel_size.x, 1);
    size.logical_pixel_size.y = std::max<size_t>(size.logical_pixel_size.y, 1);
    size.dpi_scale = std::max<double>(size.dpi_scale, 1.0);

    ff::window_size old_size = this->cached_size;
    ff::point_t<UINT> buffer_size = size.physical_pixel_size().cast<UINT>();
    this->cached_size = size;
    this->before_resize();

    ff::log::write(ff::log::type::dx12_target, "Swap chain set size.",
        " Size=", size.logical_pixel_size.x, ",", size.logical_pixel_size.y,
        " Rotate=", size.rotated_degrees(),
        " Latency=", this->pacing_latency(),
        " Vsync=", this->pacing_vsync());

    if (this->swap_chain && old_size != size) // on game thread
    {
        ff::log::write(ff::log::type::dx12_target, "- Resize existing buffers");

        DXGI_SWAP_CHAIN_DESC1 desc;
        if (FAILED(this->swap_chain->GetDesc1(&desc)) || FAILED(this->swap_chain->ResizeBuffers(
            ff::dx12::target_window::BUFFER_COUNT, buffer_size.x, buffer_size.y, desc.Format, desc.Flags)))
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
        desc.BufferCount = ff::dx12::target_window::BUFFER_COUNT;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.Scaling = DXGI_SCALING_NONE;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        desc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

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

    if (FAILED(this->swap_chain->SetRotation(::get_dxgi_rotation(size.rotation, true))))
    {
        ff::dx12::device_fatal_error("Swap chain set rotation failed");
        return false;
    }

    if (FAILED(this->swap_chain->SetMaximumFrameLatency(this->pacing_latency())))
    {
        ff::dx12::device_fatal_error("Swap chain failed to set frame latency");
        return false;
    }

    this->latency_handle = ff::win_handle(this->swap_chain->GetFrameLatencyWaitableObject());
    assert(this->target_textures.empty());

    for (size_t i = 0; i < ff::dx12::target_window::BUFFER_COUNT; i++)
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        if (FAILED(this->swap_chain->GetBuffer(static_cast<UINT>(i), IID_PPV_ARGS(&resource))))
        {
            ff::dx12::device_fatal_error("Swap chain get buffer failed");
            return false;
        }

        this->target_textures.emplace_back(ff::string::concat("Swap chain back buffer ", i), resource.Get());
        this->target_textures.back().create_target_view(this->target_views.cpu_handle(i));
    }

    return true;
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

void ff::dx12::target_window::before_reset()
{
    this->pacing = {};
    this->latency_handle.close();
    this->target_textures.clear();
}

bool ff::dx12::target_window::reset()
{
    return this->internal_reset();
}

void ff::dx12::target_window::notify_window_message(ff::window* window, ff::window_message& msg)
{
    check_ret(this->window);
    assert(this->window == window);

    switch (msg.msg)
    {
        case WM_ENTERSIZEMOVE:
            this->resizing_data = { true };
            break;

        case WM_EXITSIZEMOVE:
            if (this->resizing_data.resizing)
            {
                if (this->resizing_data.size_valid && this->cached_size != this->resizing_data.size)
                {
                    ff::dxgi::defer_resize_target(this, this->resizing_data.size);
                }

                this->resizing_data = {};
                this->pacing = {};
            }
            break;

        case WM_SIZE:
            if (msg.wp != SIZE_MINIMIZED)
            {
                ff::window_size size = this->window->size();
                if (this->resizing_data.resizing)
                {
                    this->resizing_data.size_valid = true;
                    this->resizing_data.size = size;
                }
                else if (this->cached_size != size)
                {
                    ff::dxgi::defer_resize_target(this, size);
                }
            }
            break;

        case WM_DESTROY:
            this->window = nullptr;
            break;

        case WM_SYSKEYDOWN:
            if (msg.wp == VK_BACK && ff::constants::debug_build)
            {
                if (::GetKeyState(VK_SHIFT) < 0)
                {
                    ff::thread_dispatch::get_game()->post([]
                    {
                        ff::dx12::device_fatal_error("Pretend DX12 device fatal error for testing");
                    });
                }
                else
                {
                    ff::dxgi::defer_reset_device(true);
                }
            }
            break;
    }
}

uint32_t ff::dx12::target_window::pacing_latency() const
{
    return ff::dx12::target_window::pacing_stages[this->pacing.stage].latency;
}

bool ff::dx12::target_window::pacing_vsync() const
{
    return ff::dx12::target_window::pacing_stages[this->pacing.stage].vsync;
}
