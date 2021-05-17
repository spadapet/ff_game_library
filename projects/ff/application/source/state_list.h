#pragma once

#include "state.h"

namespace ff
{
    class state_list : public ff::state
    {
    public:
        state_list() = default;
        state_list(std::vector<std::shared_ptr<ff::state>>&& states);
        state_list(state_list&& other) noexcept = default;
        state_list(const state_list& other) = default;

        state_list& operator=(state_list&& other) noexcept = default;
        state_list& operator=(const state_list& other) = default;

        void push(std::shared_ptr<ff::state> state);

        virtual size_t child_state_count() override;
        virtual ff::state* child_state(size_t index) override;

    private:
        std::vector<std::shared_ptr<ff::state>> states;
    };
}
