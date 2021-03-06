#pragma once

#include "state.h"

namespace ff
{
    struct init_app_params
    {
        std::function<void()> game_thread_started_func;
        std::function<void()> game_thread_finished_func;
        std::function<std::shared_ptr<ff::state>()> create_initial_state_func;
        std::function<double()> get_time_scale_func;
        std::function<ff::state::advance_t()> get_advance_type_func;
#if UWP_APP
        bool use_swap_chain_panel;
#endif
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
