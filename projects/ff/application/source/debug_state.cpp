#include "pch.h"
#include "app.h"
#include "app_time.h"
#include "debug_state.h"

void ff::add_debug_pages(ff::debug_pages_base* pages)
{}

void ff::remove_debug_pages(ff::debug_pages_base* pages)
{}

std::shared_ptr<ff::state> ff::debug_state::advance_time()
{
    return nullptr;
}

void ff::debug_state::advance_input()
{}

void ff::debug_state::render(ff::dx11_target_base& target, ff::dx11_depth& depth)
{
    if (!this->draw_device)
    {
        this->draw_device = ff::draw_device::create();
    }

    ff::rect_fixed rect(ff::point_fixed(0, 0), target.size().rotated_pixel_size().cast<ff::fixed_int>());
    ff::draw_ptr draw = this->draw_device->begin_draw(target, &depth, rect, rect);
    if (draw)
    {
        ff::fixed_int x = static_cast<int>(ff::app_time().advance_count % 256 + 32);
        draw->draw_outline_circle(ff::point_fixed(x, 32), 16, ff::color::red(), 4);
    }
}

void ff::debug_state::frame_rendered(ff::state::advance_t type, ff::dx11_target_base& target, ff::dx11_depth& depth)
{}

ff::state::status_t ff::debug_state::status()
{
    return ff::state::status_t::alive; //TODO: ignore;
}
