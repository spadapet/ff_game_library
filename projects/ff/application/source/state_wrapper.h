#pragma once

#include "state.h"

namespace ff
{
    class state_wrapper : public ff::state
    {
    public:
        state_wrapper() = default;
        state_wrapper(state_wrapper&& other) noexcept = default;
        state_wrapper(const state_wrapper& other) = delete;
        state_wrapper(std::shared_ptr<ff::state> state);

        state_wrapper& operator=(state_wrapper&& other) noexcept = default;
        state_wrapper& operator=(const state_wrapper& other) = delete;
        state_wrapper& operator=(std::shared_ptr<ff::state> state);
        operator bool() const;

        void reset();
        const std::shared_ptr<ff::state>& wrapped_state() const;

        virtual std::shared_ptr<ff::state> advance_time() override;
        virtual void advance_input() override;
        virtual void render(ff::target_base& target, ff::dx11_depth& depth) override;
        virtual void render() override;

        virtual void frame_started(ff::state::advance_t type) override;
        virtual void frame_rendering(ff::state::advance_t type) override;
        virtual void frame_rendered(ff::state::advance_t type, ff::target_base& target, ff::dx11_depth& depth) override;

        virtual ff::state::cursor_t cursor() override;
        virtual std::shared_ptr<ff::state_wrapper> wrap() override;
        virtual std::shared_ptr<ff::state> unwrap() override;

    private:
        std::shared_ptr<ff::state> state;
    };
}
