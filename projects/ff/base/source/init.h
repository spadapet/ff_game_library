#pragma once

namespace ff
{
    class init_base
    {
    public:
        init_base();
        ~init_base();

        operator bool() const;
    };

    class init_main_window
    {
    public:
        init_main_window(std::string_view title, bool visible);
        ~init_main_window();

        operator bool() const;

    private:
        ff::init_base init_base;
    };
}
