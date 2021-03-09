#pragma once

#include "state.h"

namespace ff
{
    class ui_state : public ff::state
    {
    public:
        virtual std::shared_ptr<ff::state> advance_time() override;
        virtual void advance_input() override;
        virtual void frame_rendering(ff::state::advance_t type) override;
        virtual void frame_rendered(ff::state::advance_t type, ff::dx11_target_base& target, ff::dx11_depth& depth) override;
    };
}
