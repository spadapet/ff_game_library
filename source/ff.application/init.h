#pragma once

#include "app/state.h"

namespace ff
{
    struct init_app_params
    {
        std::function<void()> register_resources_func{ &ff::init_app_params::default_empty };
        std::function<void()> game_thread_started_func{ &ff::init_app_params::default_empty };
        std::function<void()> game_thread_finished_func{ &ff::init_app_params::default_empty };
        std::function<std::shared_ptr<ff::state>()> create_initial_state_func{ &ff::init_app_params::default_create_initial_state };
        std::function<double()> get_time_scale_func{ &ff::init_app_params::default_get_time_scale };
        std::function<ff::state::advance_t()> get_advance_type_func{ &ff::init_app_params::default_get_advance_type };
        std::function<bool(DirectX::XMFLOAT4&)> get_clear_color_func{ &ff::init_app_params::default_get_clear_color };

        ff::dxgi::target_window_params target_window{};

    private:
        static void default_empty();
        static std::shared_ptr<ff::state> default_create_initial_state();
        static double default_get_time_scale();
        static ff::state::advance_t default_get_advance_type();
        static bool default_get_clear_color(DirectX::XMFLOAT4&);
    };

    class init_app
    {
    public:
        init_app(const ff::init_app_params& app_params);
        ~init_app();

        operator bool() const;
    };
}
