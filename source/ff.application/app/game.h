#pragma once

#include "../init_app.h"

namespace ff
{
    struct init_game_params : public ff::init_app_params
    {
        std::function<bool(size_t)> game_debug_command_func{ [](size_t) { return false; } };

        std::string debug_input_mapping;
        bool allow_debug_commands{ ff::constants::profile_build };
        bool allow_debug_stepping{ ff::constants::profile_build };
    };

    int run_game(const ff::init_game_params& params);
}
