#pragma once

namespace ff::dx11
{
    class target_access : public ff::dxgi::target_access_base
    {
    public:
        static target_access& get(ff::dxgi::target_base& obj);

        virtual ID3D11Texture2D* target_texture() = 0;
        virtual ID3D11RenderTargetView* target_view() = 0;
    };
}
