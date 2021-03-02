#include "pch.h"
#include "init.h"
#include "ui.h"

namespace
{
    namespace shaders
    {
#include "ff.ui.res.h"

        std::shared_ptr<::ff::data_base> get_shaders_data()
        {
            return std::make_shared<::ff::data_static>(ff::build_res::bytes, ff::build_res::byte_size);
        }
    }
}

static bool init_ui_status;

namespace
{
    struct one_time_init_ui
    {
        one_time_init_ui()
        {
            ff::resource_objects::register_global_dict(::shaders::get_shaders_data());

            ::init_ui_status = ff::internal::ui::init();
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

ff::init_ui::init_ui()
{
    if (::init_ui_refs.fetch_add(1) == 0)
    {
        ::init_ui_data = std::make_unique<one_time_init_ui>();
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
    return this->init_graphics && ::init_ui_status;
}
