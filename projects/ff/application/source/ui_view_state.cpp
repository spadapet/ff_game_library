#include "pch.h"
#include "ui_view_state.h"

ff::ui_view_state::ui_view_state(std::shared_ptr<ff::ui_view> view, std::shared_ptr<ff::target_base> target_override, std::shared_ptr<ff::dx11_depth> depth_override)
    : view_(view)
    , target_override(target_override)
    , depth_override(depth_override)
{
    if (this->target_override)
    {
        auto target_window = std::dynamic_pointer_cast<ff::target_window_base>(this->target_override);
        if (target_window)
        {
            this->view_->size(*target_window);
        }
        else
        {
            this->view_->size(this->target_override->size());
        }
    }
}

const std::shared_ptr<ff::ui_view>& ff::ui_view_state::view()
{
    return this->view_;
}

std::shared_ptr<ff::state> ff::ui_view_state::advance_time()
{
    this->view_->advance();
    return nullptr;
}

void ff::ui_view_state::render(ff::target_base& target, ff::dx11_depth& depth)
{
    this->view_->render(this->target_override ? *this->target_override : target, this->depth_override ? *this->depth_override : depth);
}

void ff::ui_view_state::frame_rendering(ff::state::advance_t type)
{
    this->view_->pre_render();
}

ff::state::cursor_t ff::ui_view_state::cursor()
{
    switch (this->view_->cursor())
    {
    default:
        return ff::state::cursor_t::default;

    case Noesis::Cursor_Hand:
        return ff::state::cursor_t::hand;
    }
}
