#include "pch.h"
#include "graphics.h"
#include "init.h"
#include "shader.h"
#include "texture.h"
#include "texture_metadata.h"

#if DXVER == 11
#include "animation.h"
#include "font_file.h"
#include "palette_data.h"
#include "random_sprite.h"
#include "sprite_font.h"
#include "sprite_list.h"
#include "sprite_resource.h"
#endif

#include "ff.graphics.res.h"

static bool init_graphics_status;

namespace
{
    struct one_time_init_grahics
    {
        one_time_init_grahics()
        {
            // Resource objects
            ff::resource_object_base::register_factory<ff::internal::shader_factory>("shader");
            ff::resource_object_base::register_factory<ff::internal::texture_metadata_factory>("texture_metadata");
#if DXVER == 11
            ff::resource_object_base::register_factory<ff::internal::animation_factory>("animation");
            ff::resource_object_base::register_factory<ff::internal::font_file_factory>("font_file");
            ff::resource_object_base::register_factory<ff::internal::palette_data_factory>("palette");
            ff::resource_object_base::register_factory<ff::internal::random_sprite_factory>("random_sprite");
            ff::resource_object_base::register_factory<ff::internal::sprite_list_factory>("sprites");
            ff::resource_object_base::register_factory<ff::internal::sprite_resource_factory>("sprite_resource");
            ff::resource_object_base::register_factory<ff::internal::sprite_font_factory>("font");
            ff::resource_object_base::register_factory<ff::internal::texture_factory>("texture");
#endif
            ff::global_resources::add(::assets::graphics::data());

            ::init_graphics_status = ff::internal::graphics::init();
        }

        ~one_time_init_grahics()
        {
            ff::internal::graphics::destroy();
            ::init_graphics_status = false;
        }
    };
}

static int init_graphics_refs;
static std::unique_ptr<one_time_init_grahics> init_graphics_data;
static std::mutex init_graphics_mutex;

ff::init_graphics::init_graphics()
{
    std::scoped_lock lock(::init_graphics_mutex);

    if (::init_graphics_refs++ == 0 && this->init_resource && this->init_dx)
    {
        ::init_graphics_data = std::make_unique<one_time_init_grahics>();
    }
}

ff::init_graphics::~init_graphics()
{
    std::scoped_lock lock(::init_graphics_mutex);

    if (::init_graphics_refs-- == 1)
    {
        ::init_graphics_data.reset();
    }
}

ff::init_graphics::operator bool() const
{
    return this->init_resource && this->init_dx && ::init_graphics_status;
}