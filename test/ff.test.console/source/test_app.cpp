#include "pch.h"

void run_test_app()
{
    ff::init_app init_app(ff::init_app_params{}, ff::init_ui_params{});

    if (init_app)
    {
        ff::handle_messages_until_quit();
    }
}
