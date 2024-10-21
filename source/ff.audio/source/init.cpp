#include "pch.h"
#include "audio.h"
#include "audio_effect.h"
#include "init.h"
#include "music.h"

static bool init_audio_status;

namespace
{
    struct one_time_init_audio
    {
        one_time_init_audio()
        {
            // Resource objects
            ff::resource_object_base::register_factory<ff::internal::audio_effect_factory>("effect");
            ff::resource_object_base::register_factory<ff::internal::music_factory>("music");

            ::init_audio_status = ff::internal::audio::init();
        }

        ~one_time_init_audio()
        {
            ff::internal::audio::destroy();
            ::init_audio_status = false;
        }
    };
}

static int init_audio_refs;
static std::unique_ptr<one_time_init_audio> init_audio_data;
static std::mutex init_audio_mutex;

ff::init_audio::init_audio()
{
    std::scoped_lock lock(::init_audio_mutex);

    if (::init_audio_refs++ == 0 && this->init_base)
    {
        ::init_audio_data = std::make_unique<one_time_init_audio>();
    }
}

ff::init_audio::~init_audio()
{
    std::scoped_lock lock(::init_audio_mutex);

    if (::init_audio_refs-- == 1)
    {
        ::init_audio_data.reset();
    }
}

ff::init_audio::operator bool() const
{
    return this->init_base && ::init_audio_status;
}
