#include "pch.h"
#include "dx12/commands.h"
#include "dx12/descriptor_allocator.h"
#include "dx12/device_reset_priority.h"
#include "dx12/dx12_globals.h"
#include "dx12/resource.h"
#include "dx12/target_window.h"
#include "dx12/queue.h"
#include "dxgi/dxgi_globals.h"
#include "types/color.h"
#include "types/operators.h"

constexpr size_t MIN_BUFFER_COUNT = 2;
constexpr size_t MAX_BUFFER_COUNT = 4;

static ff::perf_counter perf_render_wait("Wait", ff::perf_color::cyan, ff::perf_chart_t::render_wait);
static ff::perf_counter perf_render_present("Present", ff::perf_color::cyan, ff::perf_chart_t::render_wait);

static size_t fix_buffer_count(size_t value)
{
    return std::clamp<size_t>(value, MIN_BUFFER_COUNT, MAX_BUFFER_COUNT);
}

static size_t fix_frame_latency(size_t value, size_t buffer_count)
{
    return std::clamp<size_t>(value, 0, ::fix_buffer_count(buffer_count));
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

ff::dx12::target_window::target_window(ff::window* window, const ff::dxgi::target_window_params& params)
    : window(window)
    , window_message_connection(window->message_sink().connect(std::bind(&target_window::handle_message, this, std::placeholders::_1, std::placeholders::_2)))
    , target_views(ff::dx12::cpu_target_descriptors().alloc_range(MAX_BUFFER_COUNT))
{
    this->init_params(params);

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
    if (this->params.extra_render_target)
    {
        return ff::dx12::target_access::get(this->extra_render_target()).dx12_target_texture();
    }

    return *this->target_textures[this->swap_chain->GetCurrentBackBufferIndex()];
}

D3D12_CPU_DESCRIPTOR_HANDLE ff::dx12::target_window::dx12_target_view()
{
    if (this->params.extra_render_target)
    {
        return ff::dx12::target_access::get(this->extra_render_target()).dx12_target_view();
    }

    return this->target_views.cpu_handle(this->swap_chain->GetCurrentBackBufferIndex());
}

void ff::dx12::target_window::clear(ff::dxgi::command_context_base& context, const DirectX::XMFLOAT4& clear_color)
{
    ff::dx12::commands::get(context).clear(*this, clear_color);
}

bool ff::dx12::target_window::begin_render(ff::dxgi::command_context_base& context, const DirectX::XMFLOAT4* clear_color)
{
    check_ret_val(*this, false);

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

bool ff::dx12::target_window::end_render(ff::dxgi::command_context_base& context)
{
    check_ret_val(*this, false);

    this->handle_latency(ff::dxgi::target_window_params::latency_strategy_t::before_execute);

    ff::dx12::commands& commands = ff::dx12::commands::get(context);
    ff::dx12::queue& queue = commands.queue();
    ff::dx12::commands* present_commands = &commands;
    ff::dx12::resource& back_buffer_resource = *this->target_textures[this->swap_chain->GetCurrentBackBufferIndex()];
    std::unique_ptr<ff::dx12::commands> new_commands;

    if (this->params.extra_render_target)
    {
        // Copy extra render target to back buffer
        ff::dxgi::target_base& extra_target = this->extra_render_target();
        ff::dx12::resource& extra_resource = ff::dx12::target_access::get(extra_target).dx12_target_texture();
        new_commands = queue.new_commands();
        present_commands = new_commands.get();
        present_commands->copy_resource(back_buffer_resource, extra_resource);

        // Finish all rendering to extra render target
        queue.execute(commands);
        this->handle_latency(ff::dxgi::target_window_params::latency_strategy_t::after_execute);
    }

    present_commands->resource_state(back_buffer_resource, D3D12_RESOURCE_STATE_PRESENT);
    queue.execute(*present_commands);

    if (!this->params.extra_render_target)
    {
        this->handle_latency(ff::dxgi::target_window_params::latency_strategy_t::after_execute);
    }

    bool success;
    {
        ff::perf_timer timer(::perf_render_present);
        const HRESULT hr = this->swap_chain->Present(this->vsync() ? 1 : 0, 0);
        success = (hr != DXGI_ERROR_DEVICE_RESET && hr != DXGI_ERROR_DEVICE_REMOVED);
    }

    if (success)
    {
        this->handle_latency(ff::dxgi::target_window_params::latency_strategy_t::after_present);
    }

    return success;
}

void ff::dx12::target_window::handle_latency(ff::dxgi::target_window_params::latency_strategy_t latency_strategy)
{
    if (latency_strategy == this->params.latency_strategy && this->frame_latency_handle)
    {
        ff::perf_timer timer(::perf_render_wait);
        this->frame_latency_handle.block();
    }
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
    size.dpi_scale = std::max<double>(size.dpi_scale, 1.0);

    ff::log::write(ff::log::type::dx12_target,
        "Swap chain set size.",
        " Size=", size.logical_pixel_size.x, ",", size.logical_pixel_size.y,
        " Rotate=", size.rotated_degrees(),
        " Buffers=", this->params.buffer_count,
        " Latency=", this->params.frame_latency,
        " VSync=", this->vsync());

    ff::window_size old_size = this->cached_size;
    std::vector<std::shared_ptr<ff::dxgi::target_base>> old_extra_render_targets;
    if (this->params.extra_render_target && old_size == size)
    {
        old_extra_render_targets = this->extra_render_targets;
    }

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
            if (i < old_extra_render_targets.size())
            {
                this->extra_render_targets.push_back(old_extra_render_targets[i]);
            }
            else
            {
                auto texture = ff::dxgi::create_render_texture(size.physical_pixel_size(), this->format(), 1, 1, 1, &ff::color_black());
                this->extra_render_targets.push_back(ff::dxgi::create_target_for_texture(texture, 0, 0, 0, size.rotation, size.dpi_scale));
            }
        }
    }

    this->size_changed_.notify(size);

    return true;
}

ff::dxgi::target_base& ff::dx12::target_window::extra_render_target()
{
    assert(this->params.extra_render_target);
    return *this->extra_render_targets[this->swap_chain->GetCurrentBackBufferIndex()];
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

bool ff::dx12::target_window::vsync() const
{
    return this->params.vsync;
}

const ff::dxgi::target_window_params& ff::dx12::target_window::init_params() const
{
    return this->params;
}

void ff::dx12::target_window::init_params(const ff::dxgi::target_window_params& params)
{
    ff::dxgi::target_window_params old_params = this->params;
    old_params.buffer_count = this->buffer_count();
    old_params.frame_latency = this->frame_latency();

    this->params = params;
    this->params.buffer_count = ::fix_buffer_count(this->params.buffer_count);
    this->params.frame_latency = ::fix_frame_latency(this->params.frame_latency, this->params.buffer_count);

    if (old_params.buffer_count != this->params.buffer_count ||
        old_params.frame_latency != this->params.frame_latency ||
        old_params.extra_render_target != this->params.extra_render_target)
    {
        this->size(this->size());
    }
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

void ff::dx12::target_window::handle_message(ff::window* window, ff::window_message& msg)
{
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
            {
                this->window_message_connection.disconnect();
                this->window = nullptr;
            }
            break;

        case WM_SYSKEYDOWN:
            if (msg.wp == VK_BACK && ff::constants::debug_build)
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
            break;
    }
}
