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
#include "graphics/animation.h"
#include "graphics/font_file.h"
#include "graphics/graphics.h"
#include "graphics/palette_data.h"
#include "graphics/random_sprite.h"
#include "graphics/shader.h"
#include "graphics/sprite_font.h"
#include "graphics/sprite_list.h"
#include "graphics/sprite_resource.h"
#include "graphics/texture_resource.h"
#include "graphics/texture_metadata.h"
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

static const ff::dxgi::host_functions& get_dxgi_host_functions()
{
    static ff::dxgi::host_functions host_functions
    {
        ff::internal::graphics::on_frame_started,
        ff::internal::graphics::on_frame_complete,
        ff::graphics::defer::set_full_screen_target,
        ff::graphics::defer::remove_target,
        ff::graphics::defer::resize_target,
        ff::graphics::defer::full_screen,
        ff::graphics::defer::reset_device,
    };

    return host_functions;
}

static const ff::dxgi::client_functions& get_dxgi_client_functions()
{
    static ff::dxgi::client_functions client_functions
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
    };

    return client_functions;
}

namespace
{
    class one_time_init_dx
    {
    public:
        one_time_init_dx()
        {
            const ff::init_window_params window_params{};
            this->init_base.init_main_window(window_params);
            assert_ret(this->init_base);

            // Resource objects
            ff::resource_object_base::register_factory<ff::internal::animation_factory>("animation");
            ff::resource_object_base::register_factory<ff::internal::audio_effect_factory>("effect");
            ff::resource_object_base::register_factory<ff::internal::font_file_factory>("font_file");
            ff::resource_object_base::register_factory<ff::internal::input_mapping_factory>("input");
            ff::resource_object_base::register_factory<ff::internal::music_factory>("music");
            ff::resource_object_base::register_factory<ff::internal::palette_data_factory>("palette");
            ff::resource_object_base::register_factory<ff::internal::random_sprite_factory>("random_sprite");
            ff::resource_object_base::register_factory<ff::internal::sprite_list_factory>("sprites");
            ff::resource_object_base::register_factory<ff::internal::sprite_resource_factory>("sprite_resource");
            ff::resource_object_base::register_factory<ff::internal::sprite_font_factory>("font");
            ff::resource_object_base::register_factory<ff::internal::shader_factory>("shader");
            ff::resource_object_base::register_factory<ff::internal::texture_factory>("texture");
            ff::resource_object_base::register_factory<ff::internal::texture_metadata_factory>("texture_metadata");

            this->init_audio_status = ff::internal::audio::init();
            this->init_input_status = ff::internal::input::init();
            this->init_dx12_status = ff::dx12::init_globals(::get_dxgi_host_functions(), D3D_FEATURE_LEVEL_11_0);
            this->init_graphics_status = ff::internal::graphics::init(::get_dxgi_client_functions());
        }

        ~one_time_init_dx()
        {
            ff::internal::graphics::destroy();
            this->init_graphics_status = false;

            ff::dx12::destroy_globals();
            this->init_dx12_status = false;

            ff::internal::input::destroy();
            this->init_input_status = false;

            ff::internal::audio::destroy();
            this->init_audio_status = false;
        }

        bool valid() const
        {
            return this->init_base &&
                this->init_audio_status &&
                this->init_input_status &&
                this->init_dx12_status &&
                this->init_graphics_status;
        }

    private:
        bool init_audio_status{};
        bool init_input_status{};
        bool init_dx12_status{};
        bool init_graphics_status{};
        ff::init_base init_base;
    };
}

static int init_dx_refs;
static std::unique_ptr<::one_time_init_dx> init_dx_data;
static std::mutex init_dx_mutex;

ff::init_dx::init_dx()
{
    std::scoped_lock lock(::init_dx_mutex);

    if (::init_dx_refs++ == 0)
    {
        ::init_dx_data = std::make_unique<::one_time_init_dx>();
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
