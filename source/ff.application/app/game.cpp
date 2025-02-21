#include "pch.h"
#include "app/app.h"
#include "app/game.h"
#include "ff.app.res.id.h"
#include "input/input.h"
#include "input/input_mapping.h"
#include "input/keyboard_device.h"
#include "input/pointer_device.h"

using namespace std::string_view_literals;

static const size_t ID_DEBUG_STEP_ONE_FRAME = ff::stable_hash_func("ff.game.step_one_frame"sv);
static const size_t ID_DEBUG_CANCEL_STEP_ONE_FRAME = ff::stable_hash_func("ff.game.cancel_step_one_frame"sv);
static const size_t ID_DEBUG_SPEED_SLOW = ff::stable_hash_func("ff.game.speed_slow"sv);
static const size_t ID_DEBUG_SPEED_FAST = ff::stable_hash_func("ff.game.speed_fast"sv);
static const size_t ID_SHOW_CUSTOM_DEBUG = ff::stable_hash_func("ff.game.show_custom_debug"sv);

static ff::init_game_params game_params;
static bool debug_step_one_frame{};
static bool debug_stepping_frames{};
static double debug_time_scale{ 1.0 };
static std::array<ff::auto_resource<ff::input_mapping>, 2> debug_input_mapping;
static std::array<std::unique_ptr<ff::input_event_provider>, 2> debug_input_events;

static void game_debug_command(size_t id)
{
    check_ret(::game_params.allow_debug_commands && ::game_params.game_debug_command_func(id));

    if (::game_params.allow_debug_stepping)
    {
        if (id == ::ID_DEBUG_CANCEL_STEP_ONE_FRAME)
        {
            ::debug_step_one_frame = false;
            ::debug_stepping_frames = false;
        }
        else if (id == ::ID_DEBUG_STEP_ONE_FRAME)
        {
            ::debug_step_one_frame = ::debug_stepping_frames;
            ::debug_stepping_frames = true;
        }
    }
}

static double game_time_scale()
{
    return ::game_params.game_time_scale_func() * ::debug_time_scale;
}

static ff::app_update_t game_update_type()
{
    if (::debug_step_one_frame)
    {
        return ff::app_update_t::single_step;
    }

    if (::debug_stepping_frames || ff::global_resources::is_rebuilding())
    {
        return ff::app_update_t::stopped;
    }

    return ::game_params.game_update_type_func();
}

static void game_input()
{
    if (::game_params.allow_debug_commands)
    {
        if (!::debug_input_events[0])
        {
            ::debug_input_events[0] = std::make_unique<ff::input_event_provider>(*::debug_input_mapping[0].object(),
                std::vector<const ff::input_vk*>{ &ff::input::keyboard(), &ff::input::pointer() });

            if (::debug_input_mapping[1].object())
            {
                ::debug_input_events[1] = std::make_unique<ff::input_event_provider>(*::debug_input_mapping[1].object(),
                    std::vector<const ff::input_vk*>{ &ff::input::keyboard(), &ff::input::pointer() });
            }
        }

        if (::debug_input_events[0]->update())
        {
            for (const ff::input_event& event_ : ::debug_input_events[0]->events())
            {
                if (event_.count >= 1)
                {
                    ::game_debug_command(event_.event_id);
                }
            }
        }

        if (::debug_input_mapping[1].object() && ::debug_input_events[1]->update())
        {
            for (const ff::input_event& event_ : ::debug_input_events[1]->events())
            {
                if (event_.count >= 1)
                {
                    ::game_debug_command(event_.event_id);
                }
            }
        }

        if (::debug_input_events[0]->digital_value(::ID_DEBUG_SPEED_FAST))
        {
            ::debug_time_scale = 4.0;
        }
        else if (::debug_input_events[0]->digital_value(::ID_DEBUG_SPEED_SLOW))
        {
            ::debug_time_scale = 0.25;
        }
        else
        {
            ::debug_time_scale = 1.0;
        }
    }
    else
    {
        ::debug_step_one_frame = false;
        ::debug_stepping_frames = false;
        ::debug_time_scale = 1.0;
    }

    ::game_params.game_input_func();
}

static void game_render(ff::app_update_t type, ff::dxgi::command_context_base& context, ff::dxgi::target_base& target)
{
    ::game_params.game_render_func(type, context, target);
    ::debug_step_one_frame = false;
}

static void game_resources_rebuilt()
{
    ::debug_input_mapping[0] = ff::internal::app::app_resources().get_resource_object(assets::app::FF_GAME_DEBUG_INPUT);

    if (!::game_params.debug_input_mapping.empty())
    {
        ::debug_input_mapping[1] = ::game_params.debug_input_mapping;
    }
    else
    {
        ::debug_input_mapping[1].reset();
    }

    ::game_params.game_resources_rebuilt();
}

int ff::run_game(const ff::init_game_params& params)
{
    ::game_params = params;

    ff::init_app_params app_params = params;
    app_params.game_time_scale_func = ::game_time_scale;
    app_params.game_update_type_func = ::game_update_type;
    app_params.game_input_func = ::game_input;
    app_params.game_render_func = ::game_render;
    app_params.game_resources_rebuilt = ::game_resources_rebuilt;

    ff::init_app init_app(params);
    assert_ret_val(init_app, 1);
    return ff::handle_messages_until_quit();
}
