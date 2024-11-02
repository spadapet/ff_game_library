#include "pch.h"
#include "dxgi/transform.h"
#include "graphics/animation_player_base.h"

bool ff::animation_player_base::advance_animation(ff::push_base<ff::animation_event>* events)
{
    return true;
}

void ff::animation_player_base::draw_animation(ff::dxgi::draw_base& draw, const ff::dxgi::pixel_transform& transform) const
{
    ff::dxgi::transform transform2 = transform;
    this->draw_animation(draw, transform2);
}
