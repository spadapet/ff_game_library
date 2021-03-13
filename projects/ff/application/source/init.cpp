#include "pch.h"
#include "app.h"
#include "init.h"

static bool init_app_status;

namespace
{
    namespace assets
    {
#include "ff.assets.res.h"

        std::shared_ptr<::ff::data_base> get_assets_data()
        {
            return std::make_shared<::ff::data_static>(ff::build_res::bytes, ff::build_res::byte_size);
        }
    }

    struct one_time_init_app
    {
        one_time_init_app(const ff::init_app_params& params)
        {
            ff::resource_objects::register_global_dict(::assets::get_assets_data());

            ::init_app_status = ff::internal::app::init(params);
        }

        ~one_time_init_app()
        {
            ff::internal::app::destroy();
            ::init_app_status = false;
        }
    };
}

static std::atomic_int init_app_refs;
static std::unique_ptr<one_time_init_app> init_app_data;

ff::init_app::init_app(const ff::init_app_params& app_params, const ff::init_ui_params& ui_params)
    : init_ui(ui_params)
{
    if (::init_app_refs.fetch_add(1) == 0 && this->init_audio && this->init_ui)
    {
        ::init_app_data = std::make_unique<one_time_init_app>(app_params);
    }
}

ff::init_app::~init_app()
{
    if (::init_app_refs.fetch_sub(1) == 1)
    {
        ::init_app_data.reset();
    }
}

ff::init_app::operator bool() const
{
    return this->init_audio && this->init_ui && ::init_app_status;
}
