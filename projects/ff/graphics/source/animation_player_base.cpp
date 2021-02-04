#include "pch.h"
#include "animation_base.h"
#include "animation_player.h"
#include "animation_player_base.h"

ff::animation_player_base::~animation_player_base()
{}

std::shared_ptr<ff::animation_player_base> ff::create_animation_player(
    const std::shared_ptr<ff::animation_base>& animation,
    float start_frame,
    float speed,
    const ff::dict* params)
{
    std::shared_ptr<ff::animation_player_base> player = std::dynamic_pointer_cast<ff::animation_player_base>(animation);
    if (!player)
    {
        player = std::make_shared<ff::animation_player>(animation, start_frame, speed, params);
    }

    return player;
}
