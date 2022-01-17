#include "pch.h"
#include "commands.h"
#include "depth.h"
#include "draw_device.h"
#include "globals.h"
#include "init.h"
#include "target_texture.h"
#include "target_window.h"
#include "texture.h"

#include "ff.dx12.res.h"

static bool init_status;

namespace
{
    struct one_time_init
    {
        one_time_init(const ff::dxgi::host_functions& host_functions, D3D_FEATURE_LEVEL feature_level)
        {
            host_functions.set_shader_resource_data(::assets::dx12::data());

            ::init_status = ff::dx12::init_globals(host_functions, feature_level);
        }

        ~one_time_init()
        {
            ff::dx12::destroy_globals();
            ::init_status = false;
        }
    };
}

static int init_refs;
static std::unique_ptr<::one_time_init> init_data;
static std::mutex init_mutex;

ff::dxgi::command_context_base& frame_context()
{
    return ff::dx12::frame_commands();
}

ff::dxgi::draw_device_base& global_draw_device()
{
    return ff::dx12::get_draw_device();
}

std::shared_ptr<ff::dxgi::texture_base> create_render_texture(ff::point_size size, DXGI_FORMAT format, size_t mip_count, size_t array_size, size_t sample_count, const DirectX::XMFLOAT4* optimized_clear_color)
{
    return std::make_shared<ff::dx12::texture>(size, format, mip_count, array_size, sample_count, optimized_clear_color);
}

std::shared_ptr<ff::dxgi::texture_base> create_static_texture(const std::shared_ptr<DirectX::ScratchImage>& data, ff::dxgi::sprite_type sprite_type)
{
    return std::make_shared<ff::dx12::texture>(data, sprite_type);
}

std::shared_ptr<ff::dxgi::depth_base> create_depth(ff::point_size size, size_t sample_count)
{
    return size
        ? std::make_shared<ff::dx12::depth>(size, sample_count)
        : std::make_shared<ff::dx12::depth>(sample_count);
}

std::shared_ptr<ff::dxgi::target_window_base> create_target_for_window(ff::window* window)
{
    return window
        ? std::make_shared<ff::dx12::target_window>(window)
        : std::make_shared<ff::dx12::target_window>();
}

std::shared_ptr<ff::dxgi::target_base> create_target_for_texture(
    const std::shared_ptr<ff::dxgi::texture_base>& texture,
    size_t array_start,
    size_t array_count,
    size_t mip_level,
    int dmdo_rotate,
    double dpi_scale)
{
    return std::make_shared<ff::dx12::target_texture>(texture, array_start, array_count, mip_level, dmdo_rotate, dpi_scale);
}

ff::dx12::init::init(const ff::dxgi::host_functions& host_functions, D3D_FEATURE_LEVEL feature_level)
    : client_functions_
{
    ff::dx12::reset_device,
    ff::dx12::trim_device,
    ff::dx12::wait_for_idle,
    ff::dx12::frame_started,
    ff::dx12::frame_complete,
    ::frame_context,
    ::global_draw_device,
    ::create_render_texture,
    ::create_static_texture,
    ::create_depth,
    ::create_target_for_window,
    ::create_target_for_texture,
    ff::dx12::create_draw_device,
}
{
    std::scoped_lock lock(::init_mutex);

    if (::init_refs++ == 0)
    {
        ::init_data = std::make_unique<::one_time_init>(host_functions, feature_level);
    }
}

ff::dx12::init::~init()
{
    std::scoped_lock lock(::init_mutex);

    if (::init_refs-- == 1)
    {
        ::init_data.reset();
    }
}

ff::dx12::init::operator bool() const
{
    return ::init_status;
}

const ff::dxgi::client_functions& ff::dx12::init::client_functions() const
{
    return this->client_functions_;
}
