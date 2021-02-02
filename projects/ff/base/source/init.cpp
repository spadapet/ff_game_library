#include "pch.h"
#include "init.h"
#include "memory.h"
#include "thread_dispatch.h"
#include "thread_pool.h"
#include "window.h"

namespace
{
    struct one_time_init_base
    {
        one_time_init_base()
        {
#if !UWP_APP
            if (!::IsMouseInPointerEnabled())
            {
                ::EnableMouseInPointer(TRUE);
            }
#endif
            ff::memory::start_tracking_allocations();
        }

        ~one_time_init_base()
        {
            ff::memory::stop_tracking_allocations();
        }

    private:
        ff::thread_dispatch thread_dispatch;
        ff::thread_pool thread_pool;
    };

    struct one_time_init_main_window
    {
        one_time_init_main_window(std::string_view title, bool visible)
#if UWP_APP
            : main_window(ff::window_type::main)
#else
            : main_window(ff::window::create_blank(ff::window_type::main, title, nullptr,
                WS_OVERLAPPEDWINDOW | (visible ? WS_VISIBLE : 0), 0,
                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT))
#endif
        {}

    private:
        ff::window main_window;
    };
}

static std::atomic_int init_base_refs;
static std::atomic_int init_main_window_refs;
static std::unique_ptr<one_time_init_base> init_base_data;
static std::unique_ptr<one_time_init_main_window> init_main_window_data;

ff::init_base::init_base()
{
    if (::init_base_refs.fetch_add(1) == 0)
    {
        ::init_base_data = std::make_unique<one_time_init_base>();
    }
}

ff::init_base::~init_base()
{
    if (::init_base_refs.fetch_sub(1) == 1)
    {
        ::init_base_data.reset();
    }
}

ff::init_base::operator bool() const
{
    return true;
}

ff::init_main_window::init_main_window(std::string_view title, bool visible)
{
    if (::init_main_window_refs.fetch_add(1) == 0)
    {
        ::init_main_window_data = std::make_unique<one_time_init_main_window>(title, visible);
    }
}

ff::init_main_window::~init_main_window()
{
    if (::init_main_window_refs.fetch_sub(1) == 1)
    {
        ::init_main_window_data.reset();
    }
}

ff::init_main_window::operator bool() const
{
    return this->init_base && ff::window::main() && ff::window::main()->operator bool();
}
