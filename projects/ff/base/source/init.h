#pragma once

namespace ff
{
    class init_base
    {
    public:
        init_base();
        ~init_base();
    };

    class init_main_window
    {
    public:
        init_main_window(std::string_view title);
        ~init_main_window();

    private:
        ff::init_base init_base;
    };
}
