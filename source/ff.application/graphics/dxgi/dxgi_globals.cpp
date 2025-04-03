#include "pch.h"
#include "graphics/dx12/depth.h" // only this interop file in DXGI knows that DX12 is used
#include "graphics/dx12/draw_device.h"
#include "graphics/dx12/dx12_globals.h"
#include "graphics/dx12/target_texture.h"
#include "graphics/dx12/target_window.h"
#include "graphics/dx12/texture.h"
#include "graphics/dxgi/dxgi_globals.h"
#include "graphics/dxgi/target_window_base.h"

namespace
{
    enum class defer_flags_t
    {
        none = 0,

        reset_check = 0x01,
        reset_force = 0x02,
        reset_bits = 0x0f,
    };
}

static std::mutex defer_mutex;
static std::vector<std::pair<ff::dxgi::target_window_base*, ff::window_size>> defer_target_size;
static std::vector<std::pair<ff::dxgi::target_window_base*, ff::dxgi::target_window_params>> defer_target_reset;
static ::defer_flags_t defer_flags;
static DXGI_ADAPTER_DESC user_selected_adapter_desc{};

void ff::dxgi::remove_target(ff::dxgi::target_window_base* target)
{
    std::scoped_lock lock(::defer_mutex);
    assert_ret(target);

    std::erase_if(::defer_target_size, [target](const auto& pair) { return pair.first == target; });
    std::erase_if(::defer_target_reset, [target](const auto& pair) { return pair.first == target; });
}

void ff::dxgi::defer_resize_target(ff::dxgi::target_window_base* target, const ff::window_size& size)
{
    assert_ret(target);

    std::scoped_lock lock(::defer_mutex);

    for (auto& i : ::defer_target_size)
    {
        if (i.first == target)
        {
            i.second = size;
            return;
        }
    }

    ::defer_target_size.push_back(std::make_pair(target, size));
}

void ff::dxgi::defer_reset_target(ff::dxgi::target_window_base* target, const ff::dxgi::target_window_params& params)
{
    assert_ret(target);

    std::scoped_lock lock(::defer_mutex);

    for (auto& i : ::defer_target_reset)
    {
        if (i.first == target)
        {
            i.second = params;
            return;
        }
    }

    ::defer_target_reset.push_back(std::make_pair(target, params));
}

void ff::dxgi::defer_reset_device(bool force)
{
    std::scoped_lock lock(::defer_mutex);

    ::defer_flags = ff::flags::set(
        ff::flags::clear(::defer_flags, ::defer_flags_t::reset_bits),
        force ? ::defer_flags_t::reset_force : ::defer_flags_t::reset_check);
}

void ff::dxgi::flush_commands()
{
    std::unique_lock lock(::defer_mutex);

    while (::defer_flags != ::defer_flags_t::none ||
        !::defer_target_size.empty() ||
        !::defer_target_reset.empty())
    {
        if (ff::flags::has_any(::defer_flags, ::defer_flags_t::reset_bits))
        {
            bool force = ff::flags::has(::defer_flags, ::defer_flags_t::reset_force);
            ::defer_flags = ff::flags::clear(::defer_flags, ::defer_flags_t::reset_bits);
            lock.unlock();

            ff::dxgi::reset_device(force);
            lock.lock();
        }

        if (!::defer_target_size.empty())
        {
            const auto defer_sizes = std::move(::defer_target_size);
            lock.unlock();

            for (const auto& i : defer_sizes)
            {
                i.first->size(i.second);
            }

            lock.lock();
        }

        if (!::defer_target_reset.empty())
        {
            const auto defer_reset = std::move(::defer_target_reset);
            lock.unlock();

            for (const auto& i : defer_reset)
            {
                i.first->init_params(i.second);
            }

            lock.lock();
        }
    }
}

// All of the following implementations are DX12-only for now

bool ff::dxgi::reset_device(bool force)
{
    return ff::dx12::reset_device(force);
}

void ff::dxgi::trim_device()
{
    ff::dx12::trim_device();
}

void ff::dxgi::wait_for_idle()
{
    ff::dx12::wait_for_idle();
}

ff::dxgi::command_context_base& ff::dxgi::frame_started()
{
    return ff::dx12::frame_started();
}

void ff::dxgi::frame_flush()
{
    ff::dx12::frame_flush();
}

void ff::dxgi::frame_complete()
{
    ff::dx12::frame_complete();
}

ff::dxgi::draw_device_base& ff::dxgi::global_draw_device()
{
    return ff::dx12::get_draw_device();
}

IDXGIFactory6* ff::dxgi::factory()
{
    return ff::dx12::factory();
}

IDXGIAdapter3* ff::dxgi::adapter()
{
    return ff::dx12::adapter();
}

static bool adapter_matches(IDXGIAdapter3* adapter, const DXGI_ADAPTER_DESC& match_desc)
{
    DXGI_ADAPTER_DESC desc{};
    if ((match_desc.AdapterLuid.LowPart || match_desc.AdapterLuid.HighPart) && SUCCEEDED(adapter->GetDesc(&desc)))
    {
        if (desc.AdapterLuid.LowPart == match_desc.AdapterLuid.LowPart &&
            desc.AdapterLuid.HighPart == match_desc.AdapterLuid.HighPart)
        {
            return true;
        }

        if (desc.VendorId == match_desc.VendorId &&
            desc.DeviceId == match_desc.DeviceId &&
            desc.SubSysId == match_desc.SubSysId &&
            desc.Revision == match_desc.Revision)
        {
            return true;
        }

        if (std::wcscmp(desc.Description, match_desc.Description) == 0 &&
            desc.DedicatedVideoMemory == match_desc.DedicatedVideoMemory)
        {
            return true;
        }
    }

    return false;
}

std::vector<Microsoft::WRL::ComPtr<IDXGIAdapter3>> ff::dxgi::enum_adapters(DXGI_GPU_PREFERENCE gpu_preference, size_t& out_best_choice)
{
    out_best_choice = 0;

    std::vector<Microsoft::WRL::ComPtr<IDXGIAdapter3>> result;
    result.reserve(8);

    bool found_warp = false;
    bool warp_adapter_valid = false;
    Microsoft::WRL::ComPtr<IDXGIAdapter3> warp_adapter;
    DXGI_ADAPTER_DESC warp_desc{};

    if (SUCCEEDED(ff::dxgi::factory()->EnumWarpAdapter(IID_PPV_ARGS(&warp_adapter))) && SUCCEEDED(warp_adapter->GetDesc(&warp_desc)))
    {
        warp_adapter_valid = true;
    }

    for (size_t i = 0; ; i++)
    {
        Microsoft::WRL::ComPtr<IDXGIAdapter3> adapter;
        DXGI_ADAPTER_DESC desc{};

        if (SUCCEEDED(ff::dxgi::factory()->EnumAdapterByGpuPreference(static_cast<UINT>(i), gpu_preference, IID_PPV_ARGS(&adapter))) &&
            SUCCEEDED(adapter->GetDesc(&desc)))
        {
            if (!found_warp && warp_adapter_valid && desc.VendorId == warp_desc.VendorId && desc.DeviceId == warp_desc.DeviceId)
            {
                found_warp = true;
            }

            ff::log::write(ff::log::type::dx12, "Adapter[", result.size(), "] = ", ff::dxgi::adapter_name(desc));
            result.push_back(std::move(adapter));
        }
        else
        {
            break;
        }
    }

    if (warp_adapter_valid && !found_warp)
    {
        result.push_back(std::move(warp_adapter));
    }

    for (size_t i = 0; i < result.size(); i++)
    {
        if (::adapter_matches(result[i].Get(), ::user_selected_adapter_desc))
        {
            out_best_choice = i;
            break;
        }
    }

    return result;
}

void ff::dxgi::user_selected_adapter(const DXGI_ADAPTER_DESC& desc, bool reset_now)
{
    ::user_selected_adapter_desc = desc;
}

const DXGI_ADAPTER_DESC& ff::dxgi::user_selected_adapter()
{
    return ::user_selected_adapter_desc;
}

std::string ff::dxgi::adapter_name(IDXGIAdapter3* adapter)
{
    DXGI_ADAPTER_DESC desc{};
    return SUCCEEDED(adapter->GetDesc(&desc)) ? ff::dxgi::adapter_name(desc) : std::string();
}

std::string ff::dxgi::adapter_name(const DXGI_ADAPTER_DESC& desc)
{
    return ff::string::to_string(std::wstring_view(&desc.Description[0]));
}

std::unique_ptr<ff::dxgi::draw_device_base> ff::dxgi::create_draw_device()
{
    return ff::dx12::create_draw_device();
}

std::shared_ptr<ff::dxgi::texture_base> ff::dxgi::create_render_texture(
    ff::point_size size,
    DXGI_FORMAT format,
    size_t mip_count,
    size_t array_size,
    size_t sample_count,
    const ff::color* optimized_clear_color)
{
    return std::make_shared<ff::dx12::texture>(size, format, mip_count, array_size, sample_count, optimized_clear_color);
}

std::shared_ptr<ff::dxgi::texture_base> ff::dxgi::create_static_texture(const std::shared_ptr<DirectX::ScratchImage>& scratch, ff::dxgi::sprite_type sprite_type)
{
    return std::make_shared<ff::dx12::texture>(scratch, sprite_type);
}

std::shared_ptr<ff::dxgi::depth_base> ff::dxgi::create_depth(ff::point_size size, size_t sample_count)
{
    return size
        ? std::make_shared<ff::dx12::depth>(size, sample_count)
        : std::make_shared<ff::dx12::depth>(sample_count);
}

std::shared_ptr<ff::dxgi::target_window_base> ff::dxgi::create_target_for_window(ff::window* window, const ff::dxgi::target_window_params& params)
{
    return std::make_shared<ff::dx12::target_window>(window, params);
}

std::shared_ptr<ff::dxgi::target_base> ff::dxgi::create_target_for_texture(
    const std::shared_ptr<ff::dxgi::texture_base>& texture,
    size_t array_start,
    size_t array_count,
    size_t mip_level,
    int dmdo_rotate,
    double dpi_scale)
{
    return std::make_shared<ff::dx12::target_texture>(texture, array_start, array_count, mip_level, dmdo_rotate, dpi_scale);
}

bool ff::internal::dxgi::init(DXGI_GPU_PREFERENCE gpu_preference, D3D_FEATURE_LEVEL feature_level)
{
    return ff::internal::dx12::init(gpu_preference, feature_level);
}

void ff::internal::dxgi::destroy()
{
    ff::internal::dx12::destroy();
}
