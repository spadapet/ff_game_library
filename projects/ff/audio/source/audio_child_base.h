#pragma once

namespace ff::internal
{
    class audio_child_base
    {
    public:
        virtual ~audio_child_base() = default;

        virtual void reset() = 0;
    };
}
