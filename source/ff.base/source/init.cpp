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
            : thread_dispatch(ff::thread_dispatch_type::main)
            , thread_pool(ff::thread_pool_type::main)
        {
#if !UWP_APP
            if (!::IsMouseInPointerEnabled())
            {
                ::EnableMouseInPointer(TRUE);
            }
#endif

#ifdef TRACK_MEMORY_ALLOCATIONS
            ff::memory::start_tracking_allocations();
#endif
        }

        ~one_time_init_base()
        {
#ifdef TRACK_MEMORY_ALLOCATIONS
            ff::memory::stop_tracking_allocations();
#endif
        }

    private:
        ff::thread_dispatch thread_dispatch;
        ff::thread_pool thread_pool;
    };

    struct one_time_init_main_window
    {
        one_time_init_main_window(const ff::init_main_window_params& params)
#if UWP_APP
            : main_window(ff::window_type::main)
#else
            : main_window(one_time_init_main_window::create_window(params))
#endif
        {}

        ~one_time_init_main_window()
        {
            // in case any background work depends on the main window
            ff::thread_pool::get()->flush();
            ff::thread_dispatch::get_main()->flush();
        }

#if !UWP_APP
        static ff::window create_window(const ff::init_main_window_params& params)
        {
            if (params.window_class.empty())
            {
                return ff::window::create_blank(ff::window_type::main, params.title, nullptr,
                    WS_OVERLAPPEDWINDOW | (params.visible ? WS_VISIBLE : 0), 0,
                    CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT);
            }

            return ff::window::create(ff::window_type::main, params.window_class, params.title, nullptr,
                WS_OVERLAPPEDWINDOW | (params.visible ? WS_VISIBLE : 0), 0,
                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT);
        }
#endif

    private:
        ff::window main_window;
    };
}

static int init_base_refs;
static int init_main_window_refs;
static std::unique_ptr<one_time_init_base> init_base_data;
static std::unique_ptr<one_time_init_main_window> init_main_window_data;
static std::mutex init_base_mutex;
static std::mutex init_main_window_mutex;

ff::init_base::init_base()
{
    std::scoped_lock lock(::init_base_mutex);

    if (::init_base_refs++ == 0)
    {
        ::init_base_data = std::make_unique<one_time_init_base>();
    }
}

ff::init_base::~init_base()
{
    std::scoped_lock lock(::init_base_mutex);

    if (::init_base_refs-- == 1)
    {
        ::init_base_data.reset();
    }
}

ff::init_base::operator bool() const
{
    return true;
}

ff::init_main_window::init_main_window(const ff::init_main_window_params& params)
{
    std::scoped_lock lock(::init_main_window_mutex);

    if (::init_main_window_refs++ == 0)
    {
        ::init_main_window_data = std::make_unique<one_time_init_main_window>(params);
    }
}

ff::init_main_window::~init_main_window()
{
    std::scoped_lock lock(::init_main_window_mutex);

    if (::init_main_window_refs-- == 1)
    {
        ::init_main_window_data.reset();
    }
}

ff::init_main_window::operator bool() const
{
    return this->init_base && ff::window::main() && ff::window::main()->operator bool();
}
