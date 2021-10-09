#pragma once

namespace ff::dx11
{
    size_t fix_sample_count(DXGI_FORMAT format, size_t sample_count);
    Microsoft::WRL::ComPtr<ID3D11Texture2D> create_texture(const DirectX::ScratchImage& data);
    D3D_SRV_DIMENSION default_shader_dimension(const D3D11_TEXTURE2D_DESC& desc);
    D3D11_RTV_DIMENSION default_target_dimension(const D3D11_TEXTURE2D_DESC& desc);
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> create_shader_view(ID3D11Texture2D* texture, size_t array_start = 0, size_t array_count = 0, size_t mip_start = 0, size_t mip_count = 0);
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> create_target_view(ID3D11Texture2D* texture, size_t array_start = 0, size_t array_count = 1, size_t mip_level = 0);
}
