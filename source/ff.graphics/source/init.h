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
        ff::init_resource init_resource;
        ff::dx12::init init_dx12;
    };
}
