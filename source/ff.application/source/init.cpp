#include "pch.h"
#include "app.h"
#include "init.h"

#include "ff.assets.res.h"

static bool init_app_status;

namespace
{
    struct one_time_init_app
    {
        one_time_init_app(const ff::init_app_params& params)
        {
            ff::global_resources::add(::assets::app::data());

            ::init_app_status = ff::internal::app::init(params);
        }

        ~one_time_init_app()
        {
            ff::internal::app::destroy();
            ::init_app_status = false;
        }
    };
}

static int init_app_refs;
static std::unique_ptr<one_time_init_app> init_app_data;
static std::mutex init_app_mutex;

ff::init_app::init_app(const ff::init_app_params& app_params, const ff::init_ui_params& ui_params)
    : init_ui(ui_params)
{
    std::scoped_lock init(::init_app_mutex);

    if (::init_app_refs++ == 0 && this->init_audio && this->init_ui)
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
    return this->init_audio && this->init_ui && ::init_app_status;
}
