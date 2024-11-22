#pragma once

namespace ff::game
{
    class app_state_base;

    struct init_params
    {
        std::function<void()> register_resources_func;
        std::function<std::shared_ptr<ff::game::app_state_base>()> create_initial_state_func;
    };

    template<class T>
    struct init_params_t : public ff::game::init_params
    {
        init_params_t()
        {
            this->create_initial_state_func = []() -> std::shared_ptr<ff::game::app_state_base>
                {
                    return std::make_shared<T>();
                };
        }
    };

    int run(const ff::game::init_params& params);

    template<class T>
    int run(std::function<void()>&& register_resources_func = {})
    {
        ff::game::init_params_t<T> params;
        params.register_resources_func = std::move(register_resources_func);
        return ff::game::run(params);
    }
}
