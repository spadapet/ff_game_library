#include "pch.h"
#include "app/app.h"
#include "init_app.h"
#include "init_dx.h"

namespace
{
    class one_time_init_app
    {
    public:
        one_time_init_app(const ff::init_app_params& params)
        {
            this->app_status = ff::internal::app::init(params, this->init_dx);
        }

        ~one_time_init_app()
        {
            ff::internal::app::destroy();
            this->app_status = false;
        }

        bool valid() const
        {
            return this->init_dx && this->app_status;
        }

    private:
        bool app_status{};

        ff::init_dx init_dx{ true };
    };
}

static int init_app_refs;
static std::unique_ptr<one_time_init_app> init_app_data;
static std::mutex init_app_mutex;

ff::init_app::init_app(const ff::init_app_params& app_params)
{
    std::scoped_lock init(::init_app_mutex);

    if (::init_app_refs++ == 0)
    {
        ::init_app_data = std::make_unique<one_time_init_app>(app_params);
    }
}

ff::init_app::~init_app()
{
    std::scoped_lock init(::init_app_mutex);

    if (::init_app_refs-- == 1)
    {
        ::init_app_data.reset();
    }
}

ff::init_app::operator bool() const
{
    return ::init_app_data && ::init_app_data->valid();
}

void ff::init_app_params::default_with_window(ff::window* window)
{
}

void ff::init_app_params::default_empty()
{
}

std::shared_ptr<ff::state> ff::init_app_params::default_create_initial_state()
{
    return {};
}

double ff::init_app_params::default_get_time_scale()
{
    return 1.0;
}

ff::state::advance_t ff::init_app_params::default_get_advance_type()
{
    return ff::state::advance_t::running;
}

bool ff::init_app_params::default_clear_back_buffer()
{
    return false;
}
