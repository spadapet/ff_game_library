#pragma once

namespace ff
{
    class dx11_texture_o;

    class dx11_texture_view_base
    {
    public:
        virtual ~dx11_texture_view_base() = default;

        virtual const dx11_texture_o* view_texture() const = 0;
        virtual ID3D11ShaderResourceView* view() = 0;
    };
}
