#pragma once

namespace ff::vertex
{
    struct line_geometry
    {
        DirectX::XMFLOAT2 position[4]; // adjacency at 0 and 3
        DirectX::XMFLOAT4 color[2];
        float thickness[2];
        float depth;
        UINT matrix_index;

#if DXVER == 11
        static const std::array<D3D11_INPUT_ELEMENT_DESC, 10>& layout();
#elif DXVER == 12
#endif
    };

    struct circle_geometry
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT4 inside_color;
        DirectX::XMFLOAT4 outside_color;
        float radius;
        float thickness;
        UINT matrix_index;

#if DXVER == 11
        static const std::array<D3D11_INPUT_ELEMENT_DESC, 6>& layout();
#elif DXVER == 12
#endif
    };

    struct triangle_geometry
    {
        DirectX::XMFLOAT2 position[3];
        DirectX::XMFLOAT4 color[3];
        float depth;
        UINT matrix_index;

#if DXVER == 11
        static const std::array<D3D11_INPUT_ELEMENT_DESC, 8>& layout();
#elif DXVER == 12
#endif
    };

    struct sprite_geometry
    {
        DirectX::XMFLOAT4 rect;
        DirectX::XMFLOAT4 uv_rect;
        DirectX::XMFLOAT4 color;
        DirectX::XMFLOAT2 scale;
        DirectX::XMFLOAT3 position;
        float rotate;
        UINT texture_index;
        UINT matrix_index;

#if DXVER == 11
        static const std::array<D3D11_INPUT_ELEMENT_DESC, 8>& layout();
#elif DXVER == 12
#endif
    };
}
