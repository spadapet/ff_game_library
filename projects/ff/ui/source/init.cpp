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

static std::atomic_int init_ui_refs;
static std::unique_ptr<one_time_init_ui> init_ui_data;

ff::init_ui::init_ui(const ff::init_ui_params& ui_params)
{
    if (::init_ui_refs.fetch_add(1) == 0 && this->init_graphics && this->init_input)
    {
        ::init_ui_data = std::make_unique<one_time_init_ui>(ui_params);
    }
}

ff::init_ui::~init_ui()
{
    if (::init_ui_refs.fetch_sub(1) == 1)
    {
        ::init_ui_data.reset();
    }
}

ff::init_ui::operator bool() const
{
    return this->init_graphics && this->init_input && ::init_ui_status;
}
