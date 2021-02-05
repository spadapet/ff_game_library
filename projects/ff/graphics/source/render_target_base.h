#pragma once

namespace ff
{
    class render_target_base
    {
    public:
        virtual ~render_target_base() = default;

        virtual DXGI_FORMAT format() const = 0;
        virtual ff::window_size size() const = 0;
        virtual void clear(const DirectX::XMFLOAT4* color = nullptr) = 0;
        virtual ID3D11Texture2D* texture() = 0;
        virtual ID3D11RenderTargetView* view() = 0;
    };
}
