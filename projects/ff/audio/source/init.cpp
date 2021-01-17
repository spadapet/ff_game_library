#include "pch.h"
#include "audio.h"
#include "audio_effect.h"
#include "init.h"
#include "music.h"

namespace
{
    struct one_time_init_audio
    {
        one_time_init_audio()
        {
            ff::audio::internal::init();

            // Resource objects
            ff::resource_object_base::register_factory<ff::internal::audio_effect_factory>("effect");
            ff::resource_object_base::register_factory<ff::internal::music_factory>("music");
        }

        ~one_time_init_audio()
        {
            ff::audio::internal::destroy();
        }
    };
}

static std::atomic_int init_audio_refs;
static std::unique_ptr<one_time_init_audio> init_audio_data;

ff::init_audio::init_audio()
{
    if (::init_audio_refs.fetch_add(1) == 0)
    {
        ::init_audio_data = std::make_unique<one_time_init_audio>();
    }
}

ff::init_audio::~init_audio()
{
    if (::init_audio_refs.fetch_sub(1) == 1)
    {
        ::init_audio_data.reset();
    }
}
