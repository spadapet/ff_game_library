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
    };
}
