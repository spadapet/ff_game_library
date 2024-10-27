#pragma once

namespace ff
{
    class window;

    struct init_window_params
    {
        std::string window_class;
        std::string title;
        bool visible{};
    };

    class init_base
    {
    public:
        init_base();
        ~init_base();

        operator bool() const;

        ff::window* init_main_window(const ff::init_window_params& window_params);
    };
}
