#include "pch.h"
#include "app/app.h"
#include "game/game_init.h"
#include "game/root_state_base.h"
#include "init_app.h"

static std::atomic_bool root_state_created{};
static std::shared_ptr<ff::game::root_state_base> root_state;
static ff::signal_connection window_connection;
static ff::window* window{};

static void window_message(ff::window* window, ff::window_message& message)
{
    if (root_state_created.load())
    {
        ::root_state->notify_window_message(window, message);
    }

    if (message.msg == WM_DESTROY)
    {
        ::window_connection.disconnect();
    }
}

void ff::game::init_params::default_empty()
{
}

std::shared_ptr<ff::game::root_state_base> ff::game::init_params::default_create_root_state()
{
    return std::make_shared<ff::game::root_state_base>();
}

void ff::game::init_params::default_with_window(ff::window* window)
{
}

static void register_resources(const ff::game::init_params& params)
{
    std::filesystem::path path = ff::filesystem::executable_path().parent_path();
    verify(ff::global_resources::add_files(path));
    params.register_resources_func();
}

static std::shared_ptr<ff::state> create_root_state(const ff::game::init_params& params)
{
    ::root_state = params.create_root_state_func();
    ::root_state->internal_init(::window);
    ::root_state_created = true;
    return ::root_state;
}

static void destroy_root_state()
{
    ::root_state.reset();
}

static double get_time_scale()
{
    return ::root_state->time_scale();
}

static ff::state::advance_t get_advance_type()
{
    return ::root_state->advance_type();
}

static bool get_clear_back_buffer()
{
    return ::root_state->clear_back_buffer();
}

int ff::game::run(const ff::game::init_params& game_params)
{
    ff::init_app_params app_params{};
    app_params.register_resources_func = [&game_params]() { ::register_resources(game_params); };
    app_params.create_initial_state_func = [&game_params]() { return ::create_root_state(game_params); };
    app_params.app_initialized_func = [&game_params](ff::window* window)
        {
            ::window_connection = window->message_sink().connect(::window_message);
            game_params.window_initialized_func(window);
        };

    app_params.game_thread_finished_func = ::destroy_root_state;
    app_params.get_time_scale_func = ::get_time_scale;
    app_params.get_advance_type_func = ::get_advance_type;
    app_params.get_clear_back_buffer = ::get_clear_back_buffer;
    app_params.target_window = game_params.target_window;

    ff::init_app init_app(app_params);
    assert_ret_val(init_app, 1);

    return ff::handle_messages_until_quit();
}
