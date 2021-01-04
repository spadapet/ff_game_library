#pragma once

namespace ff::internal
{
    class audio_child_base
    {
    public:
        virtual ~audio_child_base() = 0;

        virtual void reset() = 0;
    };
}
