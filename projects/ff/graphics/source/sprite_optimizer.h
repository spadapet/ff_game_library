#pragma once

#include "sprite.h"

namespace ff::internal
{
    std::vector<ff::sprite> optimize_sprites(const std::vector<const ff::sprite_base*>& old_sprites, DXGI_FORMAT new_format, size_t new_mip_count);
    std::vector<ff::sprite> outline_sprites(const std::vector<const ff::sprite_base*>& old_sprites, DXGI_FORMAT new_format, size_t new_mip_count);
}
