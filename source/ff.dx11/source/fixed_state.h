#pragma once

namespace ff::dx11
{
    class device_state;

    struct fixed_state
    {
        fixed_state();
        fixed_state(fixed_state&& other) noexcept = default;
        fixed_state(const fixed_state& rhs) = default;

        fixed_state& operator=(fixed_state&& rhs) noexcept = default;
        fixed_state& operator=(const fixed_state& rhs) = default;

        void apply() const;
        void apply(device_state& context) const;

        Microsoft::WRL::ComPtr<ID3D11RasterizerState> raster;
        Microsoft::WRL::ComPtr<ID3D11BlendState> blend;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depth;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> disabled_depth;
        DirectX::XMFLOAT4 blend_factor;
        UINT sample_mask;
        UINT stencil;
    };
}
