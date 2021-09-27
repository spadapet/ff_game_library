#include "pch.h"
#include "init.h"
#include "ui.h"

#include "ff.ui.res.h"

static bool init_ui_status;

namespace
{
    struct one_time_init_ui
    {
        one_time_init_ui(const ff::init_ui_params& params)
        {
            ff::global_resources::add(::assets::ui::data());

            ::init_ui_status = ff::internal::ui::init(params);
        }

        ~one_time_init_ui()
        {
            ff::internal::ui::destroy();
            ::init_ui_status = false;
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
