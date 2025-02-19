#include "pch.h"
#include "graphics/animation_player_base.h"
#include "dx_types/transform.h"

bool ff::animation_player_base::update_animation(ff::push_base<ff::animation_event>* events)
{
    return true;
}

void ff::animation_player_base::draw_animation(ff::dxgi::draw_base& draw, const ff::pixel_transform& transform) const
{
    ff::transform transform2 = transform;
    this->draw_animation(draw, transform2);
}
