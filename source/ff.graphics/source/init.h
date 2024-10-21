#pragma once

namespace ff
{
    class init_graphics
    {
    public:
        init_graphics();
        ~init_graphics();

        operator bool() const;

    private:
        ff::init_base init_base;
        ff::dx12::init init_dx12;
    };
}
