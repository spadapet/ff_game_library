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

    struct init_main_window_params
    {
        std::string window_class;
        std::string title;
        bool visible;
    };

    class init_main_window
    {
    public:
        init_main_window(const ff::init_main_window_params& params);
        ~init_main_window();

        operator bool() const;

    private:
        ff::init_base init_base;
    };

    class init_data
    {
    public:
        init_data();

        operator bool() const;

    private:
        ff::init_base init_base;
    };

    class init_resource
    {
    public:
        init_resource();
        ~init_resource();

        operator bool() const;

    private:
        ff::init_data init_data;
    };
}
