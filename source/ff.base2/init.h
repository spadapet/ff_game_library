#pragma once

namespace ff
{
    struct init_main_window_params
    {
        std::string window_class;
        std::string title;
        bool visible;
    };

    class init_base
    {
    public:
        init_base(const ff::init_main_window_params* window_params = nullptr);
        ~init_base();

        operator bool() const;
    };
}
