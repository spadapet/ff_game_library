#pragma once

#include "state.h"

namespace ff
{
    class state_wrapper : public ff::state
    {
    public:
        state_wrapper() = delete;
        state_wrapper(state_wrapper&& other) noexcept = default;
        state_wrapper(const state_wrapper& other) = delete;
        state_wrapper(std::shared_ptr<ff::state> state);

        state_wrapper& operator=(state_wrapper&& other) noexcept = default;
        state_wrapper& operator=(const state_wrapper& other) = delete;
        state_wrapper& operator=(std::shared_ptr<ff::state> state);

        virtual std::shared_ptr<ff::state> advance_time() override;
        virtual void advance_input() override;
        virtual void render(ff::dx11_target_base& target, ff::dx11_depth& depth) override;

        virtual void frame_started(ff::state::advance_t type) override;
        virtual void frame_rendering(ff::state::advance_t type) override;
        virtual void frame_rendered(ff::state::advance_t type, ff::dx11_target_base& target, ff::dx11_depth& depth) override;

        virtual void save_settings() override;
        virtual void load_settings() override;

        virtual ff::state::status_t status() override;
        virtual ff::state::cursor_t cursor() override;

    private:
        std::shared_ptr<ff::state> state;
    };
}
