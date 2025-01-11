#pragma once

#include "app/state.h"
#include "dxgi/target_window_base.h"
#include "init_dx.h"

namespace ff
{
    struct init_app_params
    {
        std::function<void(ff::window*)> app_initialized_func{ &ff::init_app_params::default_with_window };
        std::function<void()> app_destroying_func{ &ff::init_app_params::default_empty };
        std::function<void()> app_destroyed_func{ &ff::init_app_params::default_empty };
        std::function<void()> register_resources_func{ &ff::init_app_params::default_empty };
        std::function<void()> game_thread_started_func{ &ff::init_app_params::default_empty };
        std::function<void()> game_thread_finished_func{ &ff::init_app_params::default_empty };
        std::function<std::shared_ptr<ff::state>()> create_initial_state_func{ &ff::init_app_params::default_create_initial_state };
        std::function<double()> get_time_scale_func{ &ff::init_app_params::default_get_time_scale };
        std::function<ff::state::advance_t()> get_advance_type_func{ &ff::init_app_params::default_get_advance_type };
        std::function<bool()> get_clear_back_buffer{ &ff::init_app_params::default_clear_back_buffer };

        ff::init_dx_params init_dx_params{};
        ff::dxgi::target_window_params target_window{};

    private:
        static void default_with_window(ff::window* window);
        static void default_empty();
        static std::shared_ptr<ff::state> default_create_initial_state();
        static double default_get_time_scale();
        static ff::state::advance_t default_get_advance_type();
        static bool default_clear_back_buffer();
    };

    class init_app
    {
    public:
        init_app(const ff::init_app_params& app_params);
        ~init_app();

        operator bool() const;
    };
}
