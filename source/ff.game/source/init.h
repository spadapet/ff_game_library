#pragma once

namespace ff::game
{
    class app_state_base;

    struct init_params
    {
        std::function<std::shared_ptr<ff::game::app_state_base>()> create_initial_state;
        std::vector<std::function<void()>> register_global_resources;
        std::function<void()> register_noesis_components;
        std::string noesis_license_name;
        std::string noesis_license_key;
        std::string noesis_application_resources_name;
        std::string noesis_default_font;
        float noesis_default_font_size;
        bool noesis_srgb;
    };

    int run(const ff::game::init_params& params);
}
