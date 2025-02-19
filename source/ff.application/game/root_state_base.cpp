#include "pch.h"
#include "app/app.h"
#include "app/debug_stats.h"
#include "app/settings.h"
#include "ff.app.res.id.h"
#include "game/root_state_base.h"
#include "graphics/palette_data.h"
#include "input/input.h"
#include "input/keyboard_device.h"
#include "input/pointer_device.h"

using namespace std::string_view_literals;

static const size_t ID_DEBUG_STEP_ONE_FRAME = ff::stable_hash_func("ff.game.step_one_frame"sv);
static const size_t ID_DEBUG_CANCEL_STEP_ONE_FRAME = ff::stable_hash_func("ff.game.cancel_step_one_frame"sv);
static const size_t ID_DEBUG_SPEED_SLOW = ff::stable_hash_func("ff.game.speed_slow"sv);
static const size_t ID_DEBUG_SPEED_FAST = ff::stable_hash_func("ff.game.speed_fast"sv);
static const size_t ID_SHOW_CUSTOM_DEBUG = ff::stable_hash_func("ff.game.show_custom_debug"sv);
static const std::string_view ID_APP_STATE = "ff::game::app_state";
static const std::string_view ID_SYSTEM_OPTIONS = "ff::game::system_options";
static ff::game::root_state_base* global_app{};

ff::game::root_state_base::root_state_base()
{
    assert(!::global_app);
    ::global_app = this;
}

ff::game::root_state_base::root_state_base(ff::render_target&& render_target, std::initializer_list<ff::game::root_state_base::palette_t> palette_resources)
    : palette_resources(palette_resources)
    , render_target(std::make_unique<ff::render_target>(std::move(render_target)))
{
    assert(!::global_app);
    ::global_app = this;
}

ff::game::root_state_base::~root_state_base()
{
    assert(::global_app == this);
    ::global_app = nullptr;
}

ff::game::root_state_base& ff::game::root_state_base::get()
{
    assert(::global_app);
    return *::global_app;
}

void ff::game::root_state_base::internal_init(ff::window* window)
{
    this->load_settings();
    this->init_resources();
    this->init_game_state();

    this->connections.emplace_front(ff::request_save_settings_sink().connect(std::bind(&ff::game::root_state_base::on_save_settings, this)));
    this->connections.emplace_front(ff::custom_debug_sink().connect(std::bind(&ff::game::root_state_base::on_custom_debug, this)));
    this->connections.emplace_front(ff::global_resources::rebuild_end_sink().connect(std::bind(&ff::game::root_state_base::on_resources_rebuilt, this)));
}

void ff::game::root_state_base::debug_command(size_t command_id)
{
    if (this->allow_debug_commands() && !this->debug_command_override(command_id))
    {
        if (command_id == ::ID_DEBUG_CANCEL_STEP_ONE_FRAME)
        {
            this->debug_step_one_frame = false;
            this->debug_stepping_frames = false;
        }
        else if (command_id == ::ID_DEBUG_STEP_ONE_FRAME)
        {
            this->debug_step_one_frame = this->debug_stepping_frames;
            this->debug_stepping_frames = true;
        }
        else if (command_id == ::ID_SHOW_CUSTOM_DEBUG)
        {
            this->show_custom_debug();
        }
    }
}

const ff::game::system_options& ff::game::root_state_base::system_options() const
{
    return this->system_options_;
}

void ff::game::root_state_base::system_options(const ff::game::system_options& options)
{
    this->system_options_ = options;
}

double ff::game::root_state_base::time_scale()
{
    return this->debug_time_scale;
}

ff::state::advance_t ff::game::root_state_base::advance_type()
{
    if (this->debug_step_one_frame)
    {
        return ff::state::advance_t::single_step;
    }

    if (this->debug_stepping_frames || ff::global_resources::is_rebuilding())
    {
        return ff::state::advance_t::stopped;
    }

    return ff::state::advance_t::running;
}

ff::dxgi::palette_base* ff::game::root_state_base::palette(size_t index)
{
    return (index < this->palettes.size()) ? this->palettes[index].get() : nullptr;
}

bool ff::game::root_state_base::allow_debug_commands()
{
    return ff::constants::profile_build;
}

void ff::game::root_state_base::notify_window_message(ff::window* window, ff::window_message& message)
{
}

bool ff::game::root_state_base::clear_back_buffer()
{
    return true;
}

std::shared_ptr<ff::state> ff::game::root_state_base::advance_time()
{
    for (auto& i : this->palettes)
    {
        i->advance();
    }

    return ff::state::advance_time();
}

void ff::game::root_state_base::advance_input()
{
    if (this->allow_debug_commands())
    {
        if (!this->debug_input_events[0])
        {
            this->debug_input_events[0] = std::make_unique<ff::input_event_provider>(*this->debug_input_mapping[0].object(),
                std::vector<const ff::input_vk*>{ &ff::input::keyboard(), & ff::input::pointer() });

            if (this->debug_input_mapping[1].object())
            {
                this->debug_input_events[1] = std::make_unique<ff::input_event_provider>(*this->debug_input_mapping[1].object(),
                    std::vector<const ff::input_vk*>{ &ff::input::keyboard(), &ff::input::pointer() });
            }
        }

        if (this->debug_input_events[0]->advance())
        {
            for (const ff::input_event& event_ : this->debug_input_events[0]->events())
            {
                if (event_.count >= 1)
                {
                    this->debug_command(event_.event_id);
                }
            }
        }

        if (this->debug_input_mapping[1].object() && this->debug_input_events[1]->advance())
        {
            for (const ff::input_event& event_ : this->debug_input_events[1]->events())
            {
                if (event_.count >= 1)
                {
                    this->debug_command(event_.event_id);
                }
            }
        }

        if (this->debug_input_events[0]->digital_value(::ID_DEBUG_SPEED_FAST))
        {
            this->debug_time_scale = 4.0;
        }
        else if (this->debug_input_events[0]->digital_value(::ID_DEBUG_SPEED_SLOW))
        {
            this->debug_time_scale = 0.25;
        }
        else
        {
            this->debug_time_scale = 1.0;
        }
    }
    else
    {
        this->debug_step_one_frame = false;
        this->debug_stepping_frames = false;
        this->debug_time_scale = 1.0;
    }

    ff::state::advance_input();
}

void ff::game::root_state_base::render(ff::dxgi::command_context_base& context, ff::render_targets& targets)
{
    if (this->render_target)
    {
        targets.push(*this->render_target);
    }

    ff::state::render(context, targets);

    if (this->render_target)
    {
        targets.pop(context, nullptr, this->palette(0));
    }
}

void ff::game::root_state_base::frame_rendered(ff::state::advance_t type, ff::dxgi::command_context_base& context, ff::render_targets& targets)
{
    this->debug_step_one_frame = false;

    ff::state::frame_rendered(type, context, targets);
}

size_t ff::game::root_state_base::child_state_count()
{
    return this->child_state(0) ? 1 : 0;
}

ff::state* ff::game::root_state_base::child_state(size_t index)
{
    return this->game_state_.get();
}

std::shared_ptr<ff::state> ff::game::root_state_base::create_initial_game_state()
{
    return {};
}

void ff::game::root_state_base::save_settings(ff::dict& dict)
{
}

void ff::game::root_state_base::load_settings(const ff::dict& dict)
{
}

void ff::game::root_state_base::load_resources()
{
}

bool ff::game::root_state_base::debug_command_override(size_t command_id)
{
    return false;
}

void ff::game::root_state_base::show_custom_debug()
{
}

const std::shared_ptr<ff::state>& ff::game::root_state_base::game_state() const
{
    return this->game_state_;
}

void ff::game::root_state_base::load_settings()
{
    ff::dict dict = ff::settings(::ID_APP_STATE);

    if (!dict.get_struct(::ID_SYSTEM_OPTIONS, this->system_options_) ||
        this->system_options_.version != ff::game::system_options::CURRENT_VERSION)
    {
        this->system_options_ = ff::game::system_options();
    }

    this->load_settings(dict);
}

void ff::game::root_state_base::init_resources()
{
    this->debug_input_events[0].reset();
    this->debug_input_events[1].reset();
    this->debug_input_mapping[0] = ff::internal::app::app_resources().get_resource_object(assets::app::FF_GAME_DEBUG_INPUT);
    this->debug_input_mapping[1] = "game.debug_input"; // optional for the game to provide this resource

    this->palettes.clear();
    for (auto& i : this->palette_resources)
    {
        ff::auto_resource<ff::palette_data> palette_data(i.resource_name);
        this->palettes.push_back(std::make_shared<ff::palette_cycle>(palette_data.object(), i.remap_name, i.cycles_per_second));
    }

    this->load_resources();
}

void ff::game::root_state_base::init_game_state()
{
    std::shared_ptr<ff::state> state = this->create_initial_game_state();
    this->game_state_ = state ? state->wrap() : nullptr;
}

void ff::game::root_state_base::on_save_settings()
{
    ff::dict dict = ff::settings(::ID_APP_STATE);
    dict.set_struct(::ID_SYSTEM_OPTIONS, this->system_options_);

    this->save_settings(dict);
    ff::settings(::ID_APP_STATE, dict);
}

void ff::game::root_state_base::on_custom_debug()
{
    this->debug_command(::ID_SHOW_CUSTOM_DEBUG);
}

void ff::game::root_state_base::on_resources_rebuilt()
{
    this->init_resources();
}
