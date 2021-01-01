#pragma once

namespace ff::audio::internal
{
    class audio_child_base
    {
    public:
        virtual ~audio_child_base() = 0;

        virtual bool reset() = 0;
    };
}
