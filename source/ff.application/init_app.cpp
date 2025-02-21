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
            : init_dx(params.init_dx_params)
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

        ff::init_dx_async init_dx;
    };
}

static int init_app_refs;
static std::unique_ptr<one_time_init_app> init_app_data;
static std::mutex init_app_mutex;

ff::init_app::init_app(const ff::init_app_params& app_params)
{
    std::scoped_lock init(::init_app_mutex);
    assert_msg(!::init_app_refs, "Cannot init more than one app");

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
