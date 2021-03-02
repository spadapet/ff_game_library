#pragma once

namespace ff
{
    class init_ui
    {
    public:
        init_ui();
        ~init_ui();

        operator bool() const;

    private:
        ff::init_graphics init_graphics;
    };
}
