#pragma once

#include "state.h"

namespace ff
{
    class ui_view_state : public ff::state
    {
    public:
        ui_view_state(std::shared_ptr<ff::ui_view> view);
        ui_view_state(ui_view_state&& other) noexcept = default;
        ui_view_state(const ui_view_state& other) = delete;

        ui_view_state& operator=(ui_view_state&& other) noexcept = default;
        ui_view_state& operator=(const ui_view_state& other) = delete;

        const std::shared_ptr<ff::ui_view>& view();

        virtual std::shared_ptr<ff::state> advance_time() override;
        virtual void render(ff::dxgi::command_context_base& context, ff::render_targets& targets) override;
        virtual ff::state::cursor_t cursor() override;

    private:
        std::shared_ptr<ff::ui_view> view_;
    };
}
