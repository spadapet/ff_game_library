#pragma once

#include "sprite_type.h"

namespace ff::internal
{
    Microsoft::WRL::ComPtr<ID3D11Texture2D> create_texture(const DirectX::ScratchImage& data);
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> create_default_texture_view(ID3D11Texture2D* texture);
    DirectX::ScratchImage load_texture_data(const std::filesystem::path& path, DXGI_FORMAT new_format, size_t new_mip_count, DirectX::ScratchImage* palette_scratch);
    DirectX::ScratchImage convert_texture_data(const DirectX::ScratchImage& data, DXGI_FORMAT new_format, size_t new_mip_count);
    ff::sprite_type get_sprite_type(const DirectX::ScratchImage& scratch, const ff::rect_size* rect = nullptr);
    D3D_SRV_DIMENSION default_dimension(const D3D11_TEXTURE2D_DESC& desc);
}
