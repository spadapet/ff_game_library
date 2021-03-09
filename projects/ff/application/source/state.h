#pragma once

namespace ff
{
    enum class cursor_t
    {
        default,
        hand,
    };

    class state
    {
    public:
        virtual ff::cursor_t cursor();
    };
}
