#pragma once

namespace ff
{
    class texture;

    class texture_view_base
    {
    public:
        virtual ~texture_view_base() = default;

        virtual const ff::texture* view_texture() const = 0;
#if DXVER == 11
        virtual ID3D11ShaderResourceView* view() const = 0;
#elif DXVER == 12
#endif
    };
}
