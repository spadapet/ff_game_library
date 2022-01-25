#pragma once

namespace ff::game
{
    struct system_options
    {
        system_options();

        static const uint8_t CURRENT_VERSION = 2;

        uint8_t version;
        bool full_screen;
        bool sound;
        bool music;
        ff::fixed_int sound_volume;
        ff::fixed_int music_volume;
    };
}
