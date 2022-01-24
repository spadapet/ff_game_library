#include "pch.h"
#include "system_options.h"

ff::game::system_options::system_options()
    : version(ff::game::system_options::CURRENT_VERSION)
    , full_screen(false)
    , sound(true)
    , music(true)
    , sound_volume(1)
    , music_volume(1)
{}
