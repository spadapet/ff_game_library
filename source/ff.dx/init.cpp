#include "pch.h"
#include "audio/audio.h"
#include "audio/audio_effect.h"
#include "audio/music.h"
#include "dx12/commands.h"
#include "dx12/depth.h"
#include "dx12/draw_device.h"
#include "dx12/globals.h"
#include "dx12/target_texture.h"
#include "dx12/target_window.h"
#include "dx12/texture.h"
#include "dxgi/interop.h"
#include "init.h"
#include "input/input.h"
#include "input/input_mapping.h"

static ff::dxgi::command_context_base& frame_context()
{
    return ff::dx12::frame_commands();
}

static ff::dxgi::draw_device_base& global_draw_device()
{
    return ff::dx12::get_draw_device();
}

static std::shared_ptr<ff::dxgi::texture_base> create_render_texture(ff::point_size size, DXGI_FORMAT format, size_t mip_count, size_t array_size, size_t sample_count, const DirectX::XMFLOAT4* optimized_clear_color)
{
    return std::make_shared<ff::dx12::texture>(size, format, mip_count, array_size, sample_count, optimized_clear_color);
}

static std::shared_ptr<ff::dxgi::texture_base> create_static_texture(const std::shared_ptr<DirectX::ScratchImage>& data, ff::dxgi::sprite_type sprite_type)
{
    return std::make_shared<ff::dx12::texture>(data, sprite_type);
}

static std::shared_ptr<ff::dxgi::depth_base> create_depth(ff::point_size size, size_t sample_count)
{
    return size
        ? std::make_shared<ff::dx12::depth>(size, sample_count)
        : std::make_shared<ff::dx12::depth>(sample_count);
}

static std::shared_ptr<ff::dxgi::target_window_base> create_target_for_window(ff::window* window, size_t buffer_count, size_t frame_latency, bool vsync, bool allow_full_screen)
{
    window = !window ? ff::window::main() : window;
    allow_full_screen = allow_full_screen && (window == ff::window::main());
    return std::make_shared<ff::dx12::target_window>(window, buffer_count, frame_latency, vsync, allow_full_screen);
}

static std::shared_ptr<ff::dxgi::target_base> create_target_for_texture(
    const std::shared_ptr<ff::dxgi::texture_base>& texture,
    size_t array_start,
    size_t array_count,
    size_t mip_level,
    int dmdo_rotate,
    double dpi_scale)
{
    return std::make_shared<ff::dx12::target_texture>(texture, array_start, array_count, mip_level, dmdo_rotate, dpi_scale);
}

namespace
{
    class one_time_init_dx
    {
    public:
        one_time_init_dx(const ff::dxgi::host_functions& host_functions, D3D_FEATURE_LEVEL feature_level)
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
            const ff::init_window_params window_params{};
            this->init_base.init_main_window(window_params);
            assert_ret(this->init_base);

            // Resource objects
            ff::resource_object_base::register_factory<ff::internal::audio_effect_factory>("effect");
            ff::resource_object_base::register_factory<ff::internal::music_factory>("music");
            ff::resource_object_base::register_factory<ff::internal::input_mapping_factory>("input");

            this->init_audio_status = ff::internal::audio::init();
            this->init_input_status = ff::internal::input::init();
            this->init_dx12_status = ff::dx12::init_globals(host_functions, feature_level);
        }

        ~one_time_init_dx()
        {
            ff::dx12::destroy_globals();
            this->init_dx12_status = false;

            ff::internal::input::destroy();
            this->init_input_status = false;

            ff::internal::audio::destroy();
            this->init_audio_status = false;
        }

        bool valid() const
        {
            return this->init_base && this->init_audio_status && this->init_input_status;
        }

        const ff::dxgi::client_functions& client_functions() const
        {
            return this->client_functions_;
        }

    private:
        bool init_audio_status{};
        bool init_input_status{};
        bool init_dx12_status{};
        ff::init_base init_base;
        ff::dxgi::client_functions client_functions_;
    };
}

static int init_dx_refs;
static std::unique_ptr<::one_time_init_dx> init_dx_data;
static std::mutex init_dx_mutex;

ff::init_dx::init_dx(const ff::dxgi::host_functions& host_functions, D3D_FEATURE_LEVEL feature_level)
{
    std::scoped_lock lock(::init_dx_mutex);

    if (::init_dx_refs++ == 0)
    {
        ::init_dx_data = std::make_unique<::one_time_init_dx>(host_functions, feature_level);
    }
}

ff::init_dx::~init_dx()
{
    std::scoped_lock lock(::init_dx_mutex);

    if (::init_dx_refs-- == 1)
    {
        ::init_dx_data.reset();
    }
}

ff::init_dx::operator bool() const
{
    return ::init_dx_data && ::init_dx_data->valid();
}

const ff::dxgi::client_functions& ff::init_dx::client_functions() const
{
    return ::init_dx_data->client_functions();
}
