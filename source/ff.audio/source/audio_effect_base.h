#pragma once
#include "audio_child_base.h"

namespace ff
{
    class audio_playing_base;

    class audio_effect_base : public ff::internal::audio_child_base
    {
    public:
        virtual std::shared_ptr<audio_playing_base> play(bool start_now = true, float volume = 1, float speed = 1) = 0;
        virtual bool playing() const = 0;
        virtual void stop() = 0;
    };
}
