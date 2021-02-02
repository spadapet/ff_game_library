#pragma once

namespace ff
{
    class init_data
    {
    public:
        init_data();

        operator bool() const;

    private:
        ff::init_base init_base;
    };
}
