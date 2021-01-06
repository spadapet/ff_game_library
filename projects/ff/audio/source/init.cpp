#include "pch.h"
#include "audio.h"
#include "audio_effect.h"
#include "init.h"

ff::init_audio::init_audio()
{
    static struct one_time_init
    {
        one_time_init()
        {
            ff::audio::internal::init();

            // Resource objects

            ff::resource_object_base::register_factory<ff::internal::audio_effect_factory>("effect");
        }
    } init;
}

ff::init_audio::~init_audio()
{
    ff::audio::internal::destroy();
}
