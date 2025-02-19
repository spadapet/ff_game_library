#pragma once

#include "dxgi/target_window_base.h"
#include "init_dx.h"

namespace ff
{
    enum class app_update_t;

    struct init_app_params
    {
        std::function<void(ff::window*)> main_thread_initialized_func{ std::bind([] {}) };
        std::function<void(ff::window*, ff::window_message&)> main_window_message_func{ std::bind([] {}) };

        std::function<void()> game_thread_initialized_func{ [] {} };
        std::function<void()> game_thread_finished_func{ [] {} };

        std::function<double()> game_time_scale_func{ [] { return 1.0; } };
        std::function<ff::app_update_t()> game_update_type_func{ [] { return ff::app_update_t{}; } };
        std::function<bool()> game_clears_back_buffer_func{ [] { return false; } };
        std::function<void()> game_resources_rebuilt{ [] {} };

        std::function<void()> game_input_func{ [] {} };
        std::function<void()> game_update_func{ [] {} };
        std::function<void(ff::app_update_t, ff::dxgi::command_context_base&, ff::dxgi::target_base&)> game_render_func{ std::bind([] {}) };

        ff::init_dx_params init_dx_params{};
        ff::dxgi::target_window_params target_window{};
    };

    class init_app
    {
    public:
        init_app(const ff::init_app_params& app_params);
        ~init_app();

        operator bool() const;
    };
}
