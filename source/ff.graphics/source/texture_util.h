#pragma once

#include "sprite_type.h"

namespace ff::internal
{
    std::shared_ptr<DirectX::ScratchImage> load_texture_data(const ff::resource_file& resource_file, DXGI_FORMAT new_format, size_t new_mip_count, std::shared_ptr<DirectX::ScratchImage>& palette);
    std::shared_ptr<DirectX::ScratchImage> convert_texture_data(const std::shared_ptr<DirectX::ScratchImage>& data, DXGI_FORMAT new_format, size_t new_mip_count);
    ff::sprite_type get_sprite_type(const DirectX::ScratchImage& scratch, const ff::rect_size* rect = nullptr);
}
