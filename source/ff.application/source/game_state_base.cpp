#include "pch.h"
#include "app.h"
#include "debug_state.h"
#include "game_state_base.h"
#include "settings.h"

using namespace std::string_view_literals;

const size_t ff::game::app_state_base::ID_DEBUG_HIDE_UI = ff::stable_hash_func("ff::game::app_state_base::ID_DEBUG_HIDE_UI"sv);
const size_t ff::game::app_state_base::ID_DEBUG_SHOW_UI = ff::stable_hash_func("ff::game::app_state_base::ID_DEBUG_SHOW_UI"sv);
const size_t ff::game::app_state_base::ID_DEBUG_RESTART_GAME = ff::stable_hash_func("ff::game::app_state_base::ID_DEBUG_RESTART_GAME"sv);
const size_t ff::game::app_state_base::ID_DEBUG_REBUILD_RESOURCES = ff::stable_hash_func("ff::game::app_state_base::ID_DEBUG_REBUILD_RESOURCES"sv);

static const size_t ID_DEBUG_STEP_ONE_FRAME = ff::stable_hash_func("ff.game.step_one_frame"sv);
static const size_t ID_DEBUG_CANCEL_STEP_ONE_FRAME = ff::stable_hash_func("ff.game.cancel_step_one_frame"sv);
static const size_t ID_DEBUG_SPEED_SLOW = ff::stable_hash_func("ff.game.speed_slow"sv);
static const size_t ID_DEBUG_SPEED_FAST = ff::stable_hash_func("ff.game.speed_fast"sv);
static const size_t ID_SHOW_CUSTOM_DEBUG = ff::stable_hash_func("ff.game.show_custom_debug"sv);
static const std::string_view ID_APP_STATE = "ff::game::ID_APP_STATE";
static const std::string_view ID_SYSTEM_OPTIONS = "ff::game::ID_SYSTEM_OPTIONS";

ff::game::app_state_base::app_state_base(ff::render_target&& render_target, std::initializer_list<ff::game::app_state_base::palette_t> palette_resources)
    : palette_resources(palette_resources)
    , render_target(std::move(render_target))
    , debug_state_(std::make_shared<debug_state>())
{
    this->connections.emplace_front(ff::request_save_settings_sink().connect(std::bind(&ff::game::app_state_base::on_save_settings, this)));
    this->connections.emplace_front(ff::custom_debug_sink().connect(std::bind(&ff::game::app_state_base::on_custom_debug, this)));
    this->connections.emplace_front(ff::global_resources::rebuild_end_sink().connect(std::bind(&ff::game::app_state_base::on_resources_rebuilt, this)));
}

void ff::game::app_state_base::internal_init()
{
    this->load_settings();
    this->init_resources();
    this->init_game_state();
    this->apply_system_options();
}

void ff::game::app_state_base::debug_command(size_t command_id)
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
        else if (command_id == ff::game::app_state_base::ID_DEBUG_HIDE_UI)
        {
            this->show_debug_state(nullptr);
        }
        else if (command_id == ff::game::app_state_base::ID_DEBUG_SHOW_UI || command_id == ::ID_SHOW_CUSTOM_DEBUG)
        {
            this->show_debug_state(this->create_debug_overlay_state(), this->game_state_);
        }
        else if (command_id == ff::game::app_state_base::ID_DEBUG_REBUILD_RESOURCES)
        {
            if (!this->rebuilding_resources)
            {
                this->rebuilding_resources = true;
                ff::global_resources::rebuild_async();
            }
        }
        else if (command_id == ff::game::app_state_base::ID_DEBUG_RESTART_GAME)
        {
            this->init_game_state();
        }
    }
}

const ff::game::system_options& ff::game::app_state_base::system_options() const
{
    return this->system_options_;
}

void ff::game::app_state_base::system_options(const ff::game::system_options& options)
{
    this->system_options_ = options;
    this->apply_system_options();
}

ff::signal_sink<>& ff::game::app_state_base::reload_resources_sink()
{
    return this->reload_resources_signal;
}

double ff::game::app_state_base::time_scale()
{
    return this->debug_time_scale;
}

ff::state::advance_t ff::game::app_state_base::advance_type()
{
    if (this->debug_step_one_frame)
    {
        return ff::state::advance_t::single_step;
    }

    if (this->debug_stepping_frames || this->rebuilding_resources)
    {
        return ff::state::advance_t::stopped;
    }

    return ff::state::advance_t::running;
}

ff::dxgi::palette_base* ff::game::app_state_base::palette(size_t index)
{
    return (index < this->palettes.size()) ? this->palettes[index].get() : nullptr;
}

bool ff::game::app_state_base::allow_debug_commands()
{
    return ff::constants::profile_build;
}

bool ff::game::app_state_base::clear_color(DirectX::XMFLOAT4& color)
{
    color = ff::dxgi::color_black();
    return true;
}

std::shared_ptr<ff::state> ff::game::app_state_base::advance_time()
{
    for (auto& i : this->palettes)
    {
        i->advance();
    }

    if (this->pending_debug_state)
    {
        auto pending_debug_state = std::move(this->pending_debug_state);
        this->debug_state_->set(pending_debug_state->first, pending_debug_state->second);
    }

    return ff::state::advance_time();
}

void ff::game::app_state_base::advance_input()
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
                    std::vector<const ff::input_vk*>{ &ff::input::keyboard(), & ff::input::pointer() });
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

void ff::game::app_state_base::render(ff::dxgi::command_context_base& context, ff::render_targets& targets)
{
    size_t old_ui_view_count = ff::ui::rendered_views().size();

    targets.push(this->render_target);
    ff::state::render(context, targets);
    ff::rect_float target_rect = targets.pop(context, nullptr, this->palette(0));

    for (auto i = ff::ui::rendered_views().cbegin() + old_ui_view_count; i != ff::ui::rendered_views().cend(); i++)
    {
        (*i)->viewport(target_rect);
    }
}

void ff::game::app_state_base::frame_rendered(ff::state::advance_t type, ff::dxgi::command_context_base& context, ff::render_targets& targets)
{
    this->debug_step_one_frame = false;

    ff::state::frame_rendered(type, context, targets);
}

size_t ff::game::app_state_base::child_state_count()
{
    return this->child_state(0) ? 1 : 0;
}

ff::state* ff::game::app_state_base::child_state(size_t index)
{
    return this->debug_state_->visible()
        ? static_cast<ff::state*>(this->debug_state_.get())
        : static_cast<ff::state*>(this->game_state_.get());
}

std::shared_ptr<ff::state> ff::game::app_state_base::create_debug_overlay_state()
{
    return {};
}

std::shared_ptr<ff::state> ff::game::app_state_base::create_initial_game_state()
{
    return {};
}

void ff::game::app_state_base::save_settings(ff::dict& dict)
{}

void ff::game::app_state_base::load_settings(const ff::dict& dict)
{}

void ff::game::app_state_base::load_resources()
{}

bool ff::game::app_state_base::debug_command_override(size_t command_id)
{
    return false;
}

const std::shared_ptr<ff::state>& ff::game::app_state_base::game_state() const
{
    return this->game_state_;
}

void ff::game::app_state_base::show_debug_state(std::shared_ptr<ff::state> top_state, std::shared_ptr<ff::state> under_state)
{
    this->pending_debug_state = std::make_unique<std::pair<std::shared_ptr<ff::state>, std::shared_ptr<ff::state>>>(std::make_pair(top_state, under_state));
}

void ff::game::app_state_base::load_settings()
{
    ff::dict dict = ff::settings(::ID_APP_STATE);

    if (!dict.get_struct(::ID_SYSTEM_OPTIONS, this->system_options_) || this->system_options_.version != ff::game::system_options::CURRENT_VERSION)
    {
        this->system_options_ = ff::game::system_options();
    }

    this->load_settings(dict);
}

void ff::game::app_state_base::init_resources()
{
    this->debug_input_events[0].reset();
    this->debug_input_events[1].reset();
    this->debug_input_mapping[0] = "ff.game.debug_input";
    this->debug_input_mapping[1] = "game.debug_input";

    this->palettes.clear();
    for (auto& i : this->palette_resources)
    {
        ff::auto_resource<ff::palette_data> palette_data(i.resource_name);
        this->palettes.push_back(std::make_shared<ff::palette_cycle>(palette_data.object(), i.remap_name, i.cycles_per_second));
    }

    this->load_resources();
}

void ff::game::app_state_base::init_game_state()
{
    std::shared_ptr<ff::state> state = this->create_initial_game_state();
    this->game_state_ = state ? state->wrap() : nullptr;
}

void ff::game::app_state_base::apply_system_options()
{
    ff::app_render_target().full_screen(this->system_options_.full_screen);
}

void ff::game::app_state_base::on_save_settings()
{
    this->system_options_.full_screen = ff::app_render_target().full_screen();

    ff::dict dict = ff::settings(::ID_APP_STATE);
    dict.set_struct(::ID_SYSTEM_OPTIONS, this->system_options_);

    this->save_settings(dict);
    ff::settings(::ID_APP_STATE, dict);
}

void ff::game::app_state_base::on_custom_debug()
{
    if (this->debug_state_->visible())
    {
        this->debug_command(ff::game::app_state_base::ID_DEBUG_HIDE_UI);
    }
    else
    {
        this->debug_command(ff::game::app_state_base::ID_DEBUG_SHOW_UI);
    }
}

void ff::game::app_state_base::on_resources_rebuilt()
{
    this->rebuilding_resources = false;
    this->init_resources();
    this->reload_resources_signal.notify();
}

bool ff::game::app_state_base::debug_state::visible()
{
    return this->top_state != nullptr;
}

void ff::game::app_state_base::debug_state::set(std::shared_ptr<ff::state> top_state, std::shared_ptr<ff::state> under_state)
{
    this->top_state = top_state ? top_state->wrap() : nullptr;
    this->under_state = top_state ? under_state : nullptr;
}

void ff::game::app_state_base::debug_state::render(ff::dxgi::command_context_base& context, ff::render_targets& targets)
{
    if (this->visible())
    {
        if (this->under_state)
        {
            this->under_state->render(context, targets);
        }

        ff::state::render(context, targets);
    }
}

size_t ff::game::app_state_base::debug_state::child_state_count()
{
    return this->visible() ? 1 : 0;
}

ff::state* ff::game::app_state_base::debug_state::child_state(size_t index)
{
    return this->top_state.get();
}
