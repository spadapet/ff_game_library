#include "pch.h"

static const ff_string_view s_app_name = FF_SVL_INIT("ff.test.c");

int main()
{
    ff_app_init(s_app_name, s_app_name);

    ff_window_main_show();
    int exit_code = ff_window_handle_messages();

    //ff_dx12_init_params params = ff_dx12_init_params_default();
    //if (ff_dx12_init(&params))
    //{
    //    ff_dx12_destroy();
    //}

    ff_app_destroy();

    return exit_code;
}
