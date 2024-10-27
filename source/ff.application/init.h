#pragma once

#include "app/state.h"

namespace ff
{
    struct init_ui_params
    {
        std::function<const ff::dxgi::palette_base* ()> palette_func;
        std::function<void()> register_components_func;

        std::string noesis_license_name;
        std::string noesis_license_key;
        std::string application_resources_name;
    };

    class init_ui
    {
    public:
        init_ui(const ff::init_ui_params& ui_params);
        ~init_ui();

        operator bool() const;

    private:
        ff::init_input init_input;
        ff::init_graphics init_graphics;
    };

    struct init_app_params
    {
        std::function<void()> game_thread_started_func;
        std::function<void()> game_thread_finished_func;
        std::function<std::shared_ptr<ff::state>()> create_initial_state_func;
        std::function<double()> get_time_scale_func;
        std::function<ff::state::advance_t()> get_advance_type_func;
        std::function<bool(DirectX::XMFLOAT4&)> get_clear_color_func;

        // For target window
        size_t buffer_count = 2;
        size_t frame_latency = 1;
        bool vsync = true;
        bool allow_full_screen = true;
    };

    class init_app
    {
    public:
        init_app(const ff::init_app_params& app_params, const ff::init_ui_params& ui_params);
        ~init_app();

        operator bool() const;

    private:
        ff::init_audio init_audio;
        ff::init_ui init_ui;
    };
}
