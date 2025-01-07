#pragma once

#include "../app/state.h"

namespace ff::game
{
    struct system_options
    {
        static constexpr uint8_t CURRENT_VERSION = 4;

        uint8_t version{ ff::game::system_options::CURRENT_VERSION };
        bool sound{ true };
        bool music{ true };
        ff::fixed_int sound_volume{ 1 };
        ff::fixed_int music_volume{ 1 };
    };

    class root_state_base : public ff::state
    {
    public:
        struct palette_t
        {
            std::string resource_name;
            std::string remap_name;
            float cycles_per_second{};
        };

        root_state_base();
        root_state_base(ff::render_target&& render_target, std::initializer_list<ff::game::root_state_base::palette_t> palette_resources);
        virtual ~root_state_base() override;

        static ff::game::root_state_base& get();

        void internal_init(ff::window* window);
        void debug_command(size_t command_id);
        const ff::game::system_options& system_options() const;
        void system_options(const ff::game::system_options& options);

        virtual double time_scale();
        virtual ff::state::advance_t advance_type();
        virtual ff::dxgi::palette_base* palette(size_t index);
        virtual bool clear_back_buffer();
        virtual bool allow_debug_commands();
        virtual void notify_window_message(ff::window* window, ff::window_message& message);

        // ff::state
        virtual std::shared_ptr<ff::state> advance_time() override;
        virtual void advance_input() override;
        virtual void render(ff::dxgi::command_context_base& context, ff::render_targets& targets) override;
        virtual void frame_rendered(ff::state::advance_t type, ff::dxgi::command_context_base& context, ff::render_targets& targets) override;
        virtual size_t child_state_count() override;
        virtual ff::state* child_state(size_t index) override;

    protected:
        virtual std::shared_ptr<ff::state> create_initial_game_state();
        virtual void save_settings(ff::dict& dict);
        virtual void load_settings(const ff::dict& dict);
        virtual void load_resources();
        virtual bool debug_command_override(size_t command_id);
        virtual void show_custom_debug();

        const std::shared_ptr<ff::state>& game_state() const;

    private:
        void load_settings();
        void init_resources();
        void init_game_state();

        void on_save_settings();
        void on_custom_debug();
        void on_resources_rebuilt();

        // Data
        std::shared_ptr<ff::state> game_state_;
        std::forward_list<ff::signal_connection> connections;
        std::vector<ff::game::root_state_base::palette_t> palette_resources;
        std::vector<std::shared_ptr<ff::palette_cycle>> palettes;
        ff::game::system_options system_options_{};
        std::unique_ptr<ff::render_target> render_target;

        // Debugging
        std::unique_ptr<ff::input_event_provider> debug_input_events[2];
        ff::auto_resource<ff::input_mapping> debug_input_mapping[2];
        double debug_time_scale{ 1 };
        bool debug_stepping_frames{};
        bool debug_step_one_frame{};
    };
}
