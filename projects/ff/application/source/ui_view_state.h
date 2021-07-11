#pragma once

#include "state.h"

namespace ff
{
    class ui_view_state : public ff::state
    {
    public:
        ui_view_state(std::shared_ptr<ff::ui_view> view, std::shared_ptr<ff::target_base> target_override, std::shared_ptr<ff::depth> depth_override);
        ui_view_state(ui_view_state&& other) noexcept = default;
        ui_view_state(const ui_view_state& other) = delete;

        ui_view_state& operator=(ui_view_state&& other) noexcept = default;
        ui_view_state& operator=(const ui_view_state& other) = delete;

        const std::shared_ptr<ff::ui_view>& view();

        virtual std::shared_ptr<ff::state> advance_time() override;
        virtual void render(ff::target_base& target, ff::depth& depth) override;
        virtual void frame_rendering(ff::state::advance_t type) override;
        virtual ff::state::cursor_t cursor() override;

    private:
        std::shared_ptr<ff::ui_view> view_;
        std::shared_ptr<ff::target_base> target_override;
        std::shared_ptr<ff::depth> depth_override;
    };
}
