#pragma once

namespace ff
{
    class target_base
    {
    public:
        virtual ~target_base() = default;

        virtual DXGI_FORMAT format() const = 0;
        virtual ff::window_size size() const = 0;

#if DXVER == 11
        virtual ID3D11Texture2D* texture() = 0;
        virtual ID3D11RenderTargetView* view() = 0;
#elif DXVER == 12
#endif
    };
}
