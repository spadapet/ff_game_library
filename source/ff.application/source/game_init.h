#pragma once

namespace ff::game
{
    class app_state_base;

    struct init_params
    {
        std::function<std::shared_ptr<ff::game::app_state_base>()> create_initial_state;
        std::function<void()> register_global_resources;
        std::string noesis_license_name;
        std::string noesis_license_key;
        std::string noesis_application_resources_name;
    };

    template<class T>
    struct init_params_t : public ff::game::init_params
    {
        init_params_t()
        {
            this->create_initial_state = []() -> std::shared_ptr<ff::game::app_state_base>
                {
                    return std::make_shared<T>();
                };
        }
    };

    int run(const ff::game::init_params& params);
}
