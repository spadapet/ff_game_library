#pragma once

#include "../dxgi/target_window_base.h"

namespace ff
{
    struct init_game_params
    {
        std::function<void(ff::window*)> main_thread_initialized_func{ &ff::game::init_params::default_with_window };
        std::function<void()> game_thread_initialized_func{ &ff::game::init_params::default_empty };
        std::function<std::shared_ptr<ff::game::root_state_base>()> create_root_state_func{ &ff::game::init_params::default_create_root_state };

        ff::dxgi::target_window_params target_window{};

    private:
        static void default_empty();
        static std::shared_ptr<ff::game::root_state_base> default_create_root_state();
        static void default_with_window(ff::window* window);
    };

    int run(const ff::game::init_params& params);
}
