#pragma once

namespace ff
{
    class init_input
    {
    public:
        init_input();
        ~init_input();

        operator bool() const;

    private:
        ff::init_base init_base;
    };
}
