#pragma once

#include "sprite_type.h"

namespace ff::internal
{
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> create_default_texture_view(ID3D11DeviceX* device, ID3D11Texture2D* texture);
    DirectX::ScratchImage load_texture_data(const std::filesystem::path& path, DXGI_FORMAT format, size_t mips, DirectX::ScratchImage* palette_scratch);
    DirectX::ScratchImage convert_texture_data(const DirectX::ScratchImage& data, DXGI_FORMAT format, size_t mips);
    ff::sprite_type get_sprite_type(const DirectX::ScratchImage& scratch, const ff::rect_size* rect = nullptr);
}
