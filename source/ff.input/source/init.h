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
        ff::init_resource init_resource;
        ff::init_main_window init_main_window;
    };
}
