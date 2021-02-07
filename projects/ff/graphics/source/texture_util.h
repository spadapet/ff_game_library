#pragma once

#include "sprite_type.h"

namespace ff::internal
{
    const DXGI_FORMAT DEFAULT_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;
    const DXGI_FORMAT PALETTE_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> create_texture(const DirectX::ScratchImage& data);
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> create_default_texture_view(ID3D11Texture2D* texture);
    std::shared_ptr<DirectX::ScratchImage> load_texture_data(const std::filesystem::path& path, DXGI_FORMAT new_format, size_t new_mip_count, std::shared_ptr<DirectX::ScratchImage>& palette);
    std::shared_ptr<DirectX::ScratchImage> convert_texture_data(const std::shared_ptr<DirectX::ScratchImage>& data, DXGI_FORMAT new_format, size_t new_mip_count);
    DXGI_FORMAT fix_texture_format(DXGI_FORMAT format, size_t texture_width, size_t texture_height, size_t mip_count);
    ff::sprite_type get_sprite_type(const DirectX::ScratchImage& scratch, const ff::rect_size* rect = nullptr);
    D3D_SRV_DIMENSION default_shader_dimension(const D3D11_TEXTURE2D_DESC& desc);
    D3D11_RTV_DIMENSION default_render_dimension(const D3D11_TEXTURE2D_DESC& desc);
}
