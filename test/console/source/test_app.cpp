#include "pch.h"

void run_test_app()
{
    ff::init_app init_app(ff::init_app_params{}, ff::init_ui_params{});

    ff::handle_messages_until_quit();
}
