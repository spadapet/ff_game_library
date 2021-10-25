#pragma once

namespace ff::dxgi
{
    enum class sprite_type;

    std::shared_ptr<DirectX::ScratchImage> convert_texture_data(const std::shared_ptr<DirectX::ScratchImage>& data, DXGI_FORMAT new_format, size_t new_mip_count);
    ff::dxgi::sprite_type get_sprite_type(const DirectX::ScratchImage& scratch, const ff::rect_size* rect = nullptr);
}
