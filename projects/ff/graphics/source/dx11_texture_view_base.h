#pragma once

namespace ff
{
    class dx11_texture;

    class dx11_texture_view_base
    {
    public:
        virtual ~dx11_texture_view_base() = default;

        virtual const dx11_texture* view_texture() const = 0;
        virtual ID3D11ShaderResourceView* view() const = 0;
    };
}
