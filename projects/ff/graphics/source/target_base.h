#pragma once

namespace ff
{
    class target_base
    {
    public:
        virtual ~target_base() = default;

        virtual bool pre_render(const DirectX::XMFLOAT4* clear_color = nullptr) = 0;
        virtual bool post_render() = 0;
        virtual DXGI_FORMAT format() const = 0;
        virtual ff::window_size size() const = 0;

#if DXVER == 11
        virtual ID3D11Texture2D* texture() = 0;
        virtual ID3D11RenderTargetView* view() = 0;
#elif DXVER == 12
        virtual ID3D12ResourceX* texture() = 0;
        virtual D3D12_CPU_DESCRIPTOR_HANDLE view() = 0;
#endif
    };
}
