#include "pch.h"
#include "dx12/depth.h" // only this interop file in DXGI knows that DX12 is used
#include "dx12/draw_device.h"
#include "dx12/globals.h"
#include "dx12/target_texture.h"
#include "dx12/target_window.h"
#include "dx12/texture.h"
#include "dxgi/interop.h"
#include "dxgi/target_window_base.h"

namespace
{
    enum class defer_flags_t
    {
        none = 0,

        reset_check = 0x01,
        reset_force = 0x02,
        reset_bits = 0x0f,

        swap_chain_size = 0x10,
        swap_chain_bits = 0xf0,
    };
}

constexpr int SPECIAL_DMOD_FULL_SCREEN = -1;
constexpr int SPECIAL_DMOD_WINDOWED = -2;

static ff::window_size special_full_screen_size{ { 0, 0 }, 0, ::SPECIAL_DMOD_FULL_SCREEN };
static ff::window_size special_windowed_size{ { 0, 0 }, 0, ::SPECIAL_DMOD_WINDOWED };

static std::mutex defer_mutex;
static std::vector<std::pair<ff::dxgi::target_window_base*, ff::window_size>> defer_sizes;
static ::defer_flags_t defer_flags;

void ff::dxgi::remove_target(ff::dxgi::target_window_base* target)
{
    std::scoped_lock lock(::defer_mutex);
    assert_ret(target);

    for (auto i = ::defer_sizes.cbegin(); i != ::defer_sizes.cend(); i++)
    {
        if (i->first == target)
        {
            ::defer_sizes.erase(i);
            break;
        }
    }
}

void ff::dxgi::defer_resize_target(ff::dxgi::target_window_base* target, const ff::window_size& size)
{
    assert_ret(target);

    std::scoped_lock lock(::defer_mutex);

    ::defer_flags = ff::flags::set(
        ff::flags::clear(::defer_flags, ::defer_flags_t::swap_chain_bits),
        ::defer_flags_t::swap_chain_size);

    bool found_match = false;
    for (auto& i : ::defer_sizes)
    {
        if (i.first == target)
        {
            // If full screen state is already changing, don't allow size to change
            if ((i.second.rotation != ::SPECIAL_DMOD_FULL_SCREEN && i.second.rotation != ::SPECIAL_DMOD_WINDOWED) ||
                (size.rotation == ::SPECIAL_DMOD_FULL_SCREEN || size.rotation == ::SPECIAL_DMOD_WINDOWED))
            {
                i.second = size;
            }

            found_match = true;
            break;
        }
    }

    if (!found_match)
    {
        ::defer_sizes.push_back(std::make_pair(target, size));
    }
}

void ff::dxgi::defer_reset_device(bool force)
{
    std::scoped_lock lock(::defer_mutex);

    ::defer_flags = ff::flags::set(
        ff::flags::clear(::defer_flags, ::defer_flags_t::reset_bits),
        force ? ::defer_flags_t::reset_force : ::defer_flags_t::reset_check);
}

void ff::dxgi::defer_full_screen(ff::dxgi::target_window_base* target, bool value)
{
    ff::dxgi::defer_resize_target(target, value ? ::special_full_screen_size : ::special_windowed_size);
}

void ff::dxgi::flush_commands()
{
    while (::defer_flags != ::defer_flags_t::none)
    {
        std::unique_lock lock(::defer_mutex);

        if (ff::flags::has_any(::defer_flags, ::defer_flags_t::reset_bits))
        {
            bool force = ff::flags::has(::defer_flags, ::defer_flags_t::reset_force);
            ::defer_flags = ff::flags::clear(::defer_flags, ::defer_flags_t::reset_bits);
            lock.unlock();

            ff::dxgi::reset_device(force);
        }
        else if (ff::flags::has_any(::defer_flags, ::defer_flags_t::swap_chain_bits))
        {
            std::vector<std::pair<ff::dxgi::target_window_base*, ff::window_size>> defer_sizes = std::move(::defer_sizes);
            ::defer_flags = ff::flags::clear(::defer_flags, ::defer_flags_t::swap_chain_bits);
            lock.unlock();

            for (const auto& i : defer_sizes)
            {
                if (i.second.rotation == ::SPECIAL_DMOD_FULL_SCREEN ||
                    i.second.rotation == ::SPECIAL_DMOD_WINDOWED)
                {
                    if (i.first->allow_full_screen())
                    {
                        i.first->full_screen(i.second.rotation == ::SPECIAL_DMOD_FULL_SCREEN);
                    }
                }
                else
                {
                    i.first->size(i.second);
                }
            }
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

void ff::dxgi::frame_complete()
{
    ff::dx12::frame_complete();
}

ff::dxgi::draw_device_base& ff::dxgi::global_draw_device()
{
    return ff::dx12::get_draw_device();
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
    const DirectX::XMFLOAT4* optimized_clear_color)
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

bool ff::internal::dxgi::init()
{
    return ff::internal::dx12::init(D3D_FEATURE_LEVEL_11_0);
}

void ff::internal::dxgi::destroy()
{
    ff::internal::dx12::destroy();
}
