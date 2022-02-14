#pragma once

namespace ff::game
{
    class app_state_base;

    struct init_params
    {
        std::function<std::shared_ptr<ff::game::app_state_base>()> create_initial_state;
        std::function<void()> register_global_resources;
        std::function<void()> register_noesis_components;
        std::function<Noesis::Ptr<Noesis::ResourceDictionary>(std::string_view)> noesis_create_application_resources_func;
        std::string noesis_license_name;
        std::string noesis_license_key;
        std::string noesis_application_resources_name;
        std::string noesis_default_font;
        float noesis_default_font_size;
    };

    int run(const ff::game::init_params& params);
}
