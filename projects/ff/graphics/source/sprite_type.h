#pragma once

namespace ff
{
    enum class sprite_type
    {
        unknown = 0x00,
        opaque = 0x01,
        transparent = 0x02,
        palette = 0x10,

        opaque_palette = opaque | palette,
    };
}
