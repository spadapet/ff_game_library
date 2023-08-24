#include "pch.h"
#include "ui_view_state.h"

ff::ui_view_state::ui_view_state(std::shared_ptr<ff::ui_view> view, bool advance_at_frame_start)
    : view_(view)
    , advance_at_frame_start(advance_at_frame_start)
{}

const std::shared_ptr<ff::ui_view>& ff::ui_view_state::view()
{
    return this->view_;
}

void ff::ui_view_state::frame_started(ff::state::advance_t type)
{
    if (this->advance_at_frame_start)
    {
        this->view_->advance();
    }
}

std::shared_ptr<ff::state> ff::ui_view_state::advance_time()
{
    if (!this->advance_at_frame_start)
    {
        this->view_->advance();
    }

    return nullptr;
}

void ff::ui_view_state::render(ff::dxgi::command_context_base& context, ff::render_targets& targets)
{
    ff::dxgi::depth_base& depth = targets.depth(context);
    ff::dxgi::target_base& target = targets.target(context, ff::render_target_type::rgba_pma);
    ff::window_size size = target.size();

    this->view_->size(size);
    this->view_->render(context, target, depth);
}

ff::state::cursor_t ff::ui_view_state::cursor()
{
    switch (this->view_->cursor())
    {
    default:
        return ff::state::cursor_t::default_;

    case Noesis::CursorType_Hand:
        return ff::state::cursor_t::hand;
    }
}
