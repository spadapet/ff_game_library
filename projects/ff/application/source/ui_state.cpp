#include "pch.h"
#include "ui_state.h"

std::shared_ptr<ff::state> ff::ui_state::advance_time()
{
    ff::ui::state_advance_time();
    return nullptr;
}

void ff::ui_state::advance_input()
{
    ff::ui::state_advance_input();
}

void ff::ui_state::frame_rendering(ff::state::advance_t type)
{
    ff::ui::state_rendering();
}

void ff::ui_state::frame_rendered(ff::state::advance_t type, ff::dx11_target_base& target, ff::dx11_depth& depth)
{
    ff::ui::state_rendered();
}
