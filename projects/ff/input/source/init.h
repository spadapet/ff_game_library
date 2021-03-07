#pragma once

namespace ff
{
    class init_input
    {
    public:
        init_input(const ff::init_main_window_params& params);
        ~init_input();

        operator bool() const;

    private:
        ff::init_resource init_resource;
        ff::init_main_window init_main_window;
    };
}
