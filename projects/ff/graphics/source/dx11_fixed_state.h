#pragma once

namespace ff
{
    class dx11_device_state;

    struct dx11_fixed_state
    {
        dx11_fixed_state();
        dx11_fixed_state(dx11_fixed_state&& other) noexcept = default;
        dx11_fixed_state(const dx11_fixed_state& rhs) = default;

        dx11_fixed_state& operator=(dx11_fixed_state&& rhs) noexcept = default;
        dx11_fixed_state& operator=(const dx11_fixed_state& rhs) = default;

        void apply(dx11_device_state& context) const;

        Microsoft::WRL::ComPtr<ID3D11RasterizerState> raster;
        Microsoft::WRL::ComPtr<ID3D11BlendState> blend;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depth;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> disabled_depth;
        DirectX::XMFLOAT4 blend_factor;
        UINT sample_mask;
        UINT stencil;
    };
}
