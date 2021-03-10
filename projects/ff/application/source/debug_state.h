#pragma once

#include "state.h"

namespace ff
{
    class debug_pages_base;

    class debug_state : public ff::state
    {
    public:
        virtual std::shared_ptr<ff::state> advance_time() override;
        virtual void advance_input() override;
        virtual void render(ff::dx11_target_base& target, ff::dx11_depth& depth) override;
        virtual void frame_rendered(ff::state::advance_t type, ff::dx11_target_base& target, ff::dx11_depth& depth) override;
        virtual ff::state::status_t status() override;

    private:
        std::unique_ptr<ff::draw_device> draw_device;
    };

    void add_debug_pages(ff::debug_pages_base* pages);
    void remove_debug_pages(ff::debug_pages_base* pages);
}
