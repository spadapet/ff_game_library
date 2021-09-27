#pragma once

#include "sprite_type.h"

namespace ff::internal
{
    std::shared_ptr<DirectX::ScratchImage> load_texture_data(const ff::resource_file& resource_file, DXGI_FORMAT new_format, size_t new_mip_count, std::shared_ptr<DirectX::ScratchImage>& palette);
    std::shared_ptr<DirectX::ScratchImage> convert_texture_data(const std::shared_ptr<DirectX::ScratchImage>& data, DXGI_FORMAT new_format, size_t new_mip_count);
    ff::sprite_type get_sprite_type(const DirectX::ScratchImage& scratch, const ff::rect_size* rect = nullptr);

#if DXVER == 11
    Microsoft::WRL::ComPtr<ID3D11Texture2D> create_texture(const DirectX::ScratchImage& data);
    D3D_SRV_DIMENSION default_shader_dimension(const D3D11_TEXTURE2D_DESC& desc);
    D3D11_RTV_DIMENSION default_target_dimension(const D3D11_TEXTURE2D_DESC& desc);
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> create_shader_view(ID3D11Texture2D* texture, size_t array_start = 0, size_t array_count = 0, size_t mip_start = 0, size_t mip_count = 0);
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> create_target_view(ID3D11Texture2D* texture, size_t array_start = 0, size_t array_count = 1, size_t mip_level = 0);
#endif
}
