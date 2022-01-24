#pragma once

#include "system_options.h"

namespace ff::game
{
    class debug_state;

    class app_state_base : public ff::state
    {
    public:
        app_state_base();
        virtual ~app_state_base() override;

        static app_state_base* get();
        static const size_t ID_DEBUG_HIDE_UI;
        static const size_t ID_DEBUG_SHOW_UI;
        static const size_t ID_DEBUG_RESTART_GAME;
        static const size_t ID_DEBUG_REBUILD_RESOURCES;

        void init();
        const ff::game::system_options& system_options() const;
        void system_options(const ff::game::system_options& options);
        ff::signal_sink<>& reload_resources_sink();

        virtual double time_scale();
        virtual ff::state::advance_t advance_type();
        virtual ff::dxgi::palette_base* palette();
        virtual bool clear_color(DirectX::XMFLOAT4&);
        virtual bool allow_debug();
        virtual void debug_command(size_t command_id);

        // ff::state
        virtual std::shared_ptr<ff::state> advance_time() override;
        virtual void advance_input() override;
        virtual size_t child_state_count() override;
        virtual ff::state* child_state(size_t index) override;

    protected:
        virtual std::shared_ptr<ff::state> create_initial_game_state();
        virtual std::shared_ptr<ff::state> create_debug_overlay_state();
        virtual void save_settings(ff::dict& dict);
        virtual void load_settings(const ff::dict& dict);
        virtual void load_resources();

    private:
        void load_settings();
        void init_resources();
        void init_game_state();
        void apply_system_options();

        void on_save_settings();
        void on_custom_debug();
        void on_resources_rebuilt();

        // Globals
        std::shared_ptr<ff::state> game_state;
        ff::game::system_options system_options_;

        // Debugging
        std::forward_list<ff::signal_connection> connections;
        std::shared_ptr<ff::game::debug_state> debug_state;
        std::unique_ptr<ff::input_event_provider> debug_input_events;
        ff::auto_resource<ff::input_mapping> debug_input_mapping;
        ff::signal<> reload_resources_signal;
        double debug_time_scale{ 1 };
        bool debug_stepping_frames{};
        bool debug_step_one_frame{};
        bool rebuilding_resources{};
        uint8_t pending_debug_state{};
    };
}
