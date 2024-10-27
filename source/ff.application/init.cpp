#include "pch.h"
#include "app/app.h"
#include "init.h"
#include "ui/ui.h"

namespace
{
    class one_time_init_app
    {
    public:
        one_time_init_app(const ff::init_app_params& params)
        {
            this->app_status = ff::internal::app::init(params);
        }

        ~one_time_init_app()
        {
            if (this->did_init_ui)
            {
                ff::internal::ui::destroy();
                this->did_init_ui = false;
                this->ui_status = false;
            }

            ff::internal::app::destroy();
            this->app_status = false;
        }

        bool valid() const
        {
            if (!this->app_status || !this->init_audio || !this->init_input || !this->init_graphics)
            {
                return false;
            }

            if (this->did_init_ui && !this->ui_status)
            {
                return false;
            }

            return true;
        }

        bool init_ui(const ff::init_ui_params& params)
        {
            if (!this->did_init_ui)
            {
                this->did_init_ui = true;
                this->ui_status = ff::internal::ui::init(params);
            }

            return this->ui_status;
        }

    private:
        bool app_status{};
        bool did_init_ui{};
        bool ui_status{};

        ff::init_audio init_audio;
        ff::init_input init_input;
        ff::init_graphics init_graphics;
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

bool ff::init_app::init_ui(const ff::init_ui_params& ui_params)
{
    std::scoped_lock init(::init_app_mutex);
    return ::init_app_data->init_ui(ui_params);
}
