#include "pch.h"
#include "app/app.h"
#include "init.h"
#include "ui/ui.h"

static bool init_ui_status;
static bool init_app_status;

namespace
{
    struct one_time_init_ui
    {
        one_time_init_ui(const ff::init_ui_params& params)
        {
            ::init_ui_status = ff::internal::ui::init(params);
        }

        ~one_time_init_ui()
        {
            ff::internal::ui::destroy();
            ::init_ui_status = false;
        }
    };

    struct one_time_init_app
    {
        one_time_init_app(const ff::init_app_params& params)
        {
            ::init_app_status = ff::internal::app::init(params);
        }

        ~one_time_init_app()
        {
            ff::internal::app::destroy();
            ::init_app_status = false;
        }
    };
}

static int init_ui_refs;
static std::unique_ptr<one_time_init_ui> init_ui_data;
static std::mutex init_ui_mutex;

ff::init_ui::init_ui(const ff::init_ui_params& ui_params)
{
    std::scoped_lock lock(::init_ui_mutex);

    if (::init_ui_refs++ == 0 && this->init_graphics && this->init_input)
    {
        ::init_ui_data = std::make_unique<one_time_init_ui>(ui_params);
    }
}

ff::init_ui::~init_ui()
{
    std::scoped_lock lock(::init_ui_mutex);

    if (::init_ui_refs-- == 1)
    {
        ::init_ui_data.reset();
    }
}

ff::init_ui::operator bool() const
{
    return this->init_graphics && this->init_input && ::init_ui_status;
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
