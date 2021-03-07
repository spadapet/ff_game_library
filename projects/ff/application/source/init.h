#pragma once

namespace ff
{
    struct init_app_params
    {
    };

    class init_app
    {
    public:
        init_app(const ff::init_app_params& app_params, const ff::init_ui_params& ui_params, const ff::init_main_window_params& window_params);
        ~init_app();

        operator bool() const;

    private:
        ff::init_audio init_audio;
        ff::init_ui init_ui;
    };
}
