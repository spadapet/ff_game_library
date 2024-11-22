#include "pch.h"
#include "app/app.h"
#include "game/game_state_base.h"
#include "game/game_init.h"
#include "init.h"

static std::weak_ptr<ff::game::app_state_base> app_state;

static std::shared_ptr<ff::state> create_app_state(const ff::game::init_params& init_params)
{
    auto app_state = init_params.create_initial_state_func();
    ::app_state = app_state;

    if (app_state)
    {
        app_state->internal_init();
    }

    return app_state;
}

static double get_time_scale()
{
    auto app_state = ::app_state.lock();
    return app_state ? app_state->time_scale() : 1;
}

static ff::state::advance_t get_advance_type()
{
    auto app_state = ::app_state.lock();
    return app_state ? app_state->advance_type() : ff::state::advance_t::running;
}

static bool get_clear_color(DirectX::XMFLOAT4& color)
{
    auto app_state = ::app_state.lock();
    return app_state ? app_state->clear_color(color) : false;
}

static ff::init_app_params get_app_params(const ff::game::init_params& init_params)
{
    ff::init_app_params params{};
    params.register_resources_func = init_params.register_resources_func;
    params.create_initial_state_func = [&init_params]() { return ::create_app_state(init_params); };
    params.get_time_scale_func = ::get_time_scale;
    params.get_advance_type_func = ::get_advance_type;
    params.get_clear_color_func = ::get_clear_color;

    return params;
}

int ff::game::run(const ff::game::init_params& params)
{
    ff::init_app init_app(::get_app_params(params));
    assert_ret_val(init_app, 1);
    return ff::handle_messages_until_quit();
}
