#include "pch.h"
#include "base/memory.h"
#include "data_value/bool_v.h"
#include "data_value/data_v.h"
#include "data_value/dict_v.h"
#include "data_value/double_v.h"
#include "data_value/fixed_v.h"
#include "data_value/float_v.h"
#include "data_value/int_v.h"
#include "data_value/null_v.h"
#include "data_value/point_v.h"
#include "data_value/rect_v.h"
#include "data_value/resource_object_v.h"
#include "data_value/resource_v.h"
#include "data_value/saved_data_v.h"
#include "data_value/size_v.h"
#include "data_value/string_v.h"
#include "data_value/uuid_v.h"
#include "data_value/value_vector_v.h"
#include "init.h"
#include "resource/resource_file.h"
#include "resource/resource_objects.h"
#include "resource/resource_values.h"
#include "thread/thread_dispatch.h"
#include "thread/thread_pool.h"
#include "windows/window.h"

namespace
{
    struct one_time_init_base
    {
        one_time_init_base()
            : thread_dispatch(ff::thread_dispatch_type::main)
        {
            if (!::IsMouseInPointerEnabled())
            {
                ::EnableMouseInPointer(TRUE);
            }

#ifdef TRACK_MEMORY_ALLOCATIONS
            ff::memory::start_tracking_allocations();
#endif

            ff::internal::thread_pool::init();
        }

        ~one_time_init_base()
        {
            ff::internal::thread_pool::destroy();

#ifdef TRACK_MEMORY_ALLOCATIONS
            ff::memory::stop_tracking_allocations();
#endif
        }

    private:
        ff::thread_dispatch thread_dispatch;
    };

    struct one_time_init_main_window
    {
        one_time_init_main_window(const ff::init_main_window_params& params)
            : main_window(one_time_init_main_window::create_window(params))
        {}

        ~one_time_init_main_window()
        {
            // in case any background work depends on the main window
            ff::thread_pool::flush();
            ff::thread_dispatch::get_main()->flush();
        }

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

ff::init_data::init_data()
{
    static struct one_time_init
    {
        one_time_init()
        {
            ff::value::register_type<ff::type::bool_type>("bool");
            ff::value::register_type<ff::type::dict_type>("dict");
            ff::value::register_type<ff::type::data_type>("data");
            ff::value::register_type<ff::type::double_type>("double");
            ff::value::register_type<ff::type::double_vector_type>("double_vector");
            ff::value::register_type<ff::type::fixed_type>("fixed");
            ff::value::register_type<ff::type::fixed_vector_type>("fixed_vector");
            ff::value::register_type<ff::type::float_type>("float");
            ff::value::register_type<ff::type::float_vector_type>("float_vector");
            ff::value::register_type<ff::type::int_type>("int");
            ff::value::register_type<ff::type::int_vector_type>("int_vector");
            ff::value::register_type<ff::type::null_type>("null");
            ff::value::register_type<ff::type::point_double_type>("point_double");
            ff::value::register_type<ff::type::point_fixed_type>("point_fixed");
            ff::value::register_type<ff::type::point_float_type>("point_float");
            ff::value::register_type<ff::type::point_int_type>("point_int");
            ff::value::register_type<ff::type::point_size_type>("point_size");
            ff::value::register_type<ff::type::rect_double_type>("rect_double");
            ff::value::register_type<ff::type::rect_fixed_type>("rect_fixed");
            ff::value::register_type<ff::type::rect_float_type>("rect_float");
            ff::value::register_type<ff::type::rect_int_type>("rect_int");
            ff::value::register_type<ff::type::rect_size_type>("rect_size");
            ff::value::register_type<ff::type::saved_data_type>("saved_data");
            ff::value::register_type<ff::type::size_type>("size");
            ff::value::register_type<ff::type::size_vector_type>("size_vector");
            ff::value::register_type<ff::type::string_type>("string");
            ff::value::register_type<ff::type::string_vector_type>("string_vector");
            ff::value::register_type<ff::type::uuid_type>("uuid");
            ff::value::register_type<ff::type::value_vector_type>("value_vector");
        }
    } init;
}

ff::init_data::operator bool() const
{
    return this->init_base;
}

ff::init_resource::init_resource()
{
    static struct one_time_init
    {
        one_time_init()
        {
            ff::internal::global_resources::init();

            // Values
            ff::value::register_type<ff::type::resource_type>("resource");
            ff::value::register_type<ff::type::resource_object_type>("resource_object");

            // Resource objects
            ff::resource_object_base::register_factory<ff::internal::resource_file_factory>("file");
            ff::resource_object_base::register_factory<ff::internal::resource_objects_factory>("resource_objects");
            ff::resource_object_base::register_factory<ff::internal::resource_values_factory>("resource_values");
        }
    } init;
}

ff::init_resource::~init_resource()
{
    ff::internal::global_resources::destroy();
}

ff::init_resource::operator bool() const
{
    return this->init_data;
}
