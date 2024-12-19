#include "pch.h"
#include "audio/audio.h"
#include "audio/audio_effect.h"
#include "audio/music.h"
#include "dxgi/interop.h"
#include "graphics/animation.h"
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
#include "write/font_file.h"
#include "write/write.h"

namespace
{
    class one_time_init_dx
    {
    public:
        one_time_init_dx()
        {
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
            this->init_dxgi_status = ff::internal::dxgi::init();
            this->init_write_status = ff::internal::write::init();
        }

        ~one_time_init_dx()
        {
            ff::internal::write::destroy();
            this->init_write_status = false;

            ff::internal::dxgi::destroy();
            this->init_dxgi_status = false;

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
                this->init_dxgi_status &&
                this->init_write_status;
        }

    private:
        bool init_audio_status{};
        bool init_input_status{};
        bool init_dxgi_status{};
        bool init_write_status{};
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
