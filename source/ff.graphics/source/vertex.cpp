#include "pch.h"
#include "vertex.h"

const std::array<ff_dx::D3D_INPUT_ELEMENT_DESC, 10>& ff::vertex::line_geometry::layout()
{
    static const std::array<ff_dx::D3D_INPUT_ELEMENT_DESC, 10> layout
    {
        ff_dx::D3D_INPUT_ELEMENT_DESC{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, ff_dx::D3D_IPVA, 0 },
        ff_dx::D3D_INPUT_ELEMENT_DESC{ "POSITION", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 8, ff_dx::D3D_IPVA, 0 },
        ff_dx::D3D_INPUT_ELEMENT_DESC{ "POSITION", 2, DXGI_FORMAT_R32G32_FLOAT, 0, 16, ff_dx::D3D_IPVA, 0 },
        ff_dx::D3D_INPUT_ELEMENT_DESC{ "POSITION", 3, DXGI_FORMAT_R32G32_FLOAT, 0, 24, ff_dx::D3D_IPVA, 0 },
        ff_dx::D3D_INPUT_ELEMENT_DESC{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, ff_dx::D3D_IPVA, 0 },
        ff_dx::D3D_INPUT_ELEMENT_DESC{ "COLOR", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 48, ff_dx::D3D_IPVA, 0 },
        ff_dx::D3D_INPUT_ELEMENT_DESC{ "THICK", 0, DXGI_FORMAT_R32_FLOAT, 0, 64, ff_dx::D3D_IPVA, 0 },
        ff_dx::D3D_INPUT_ELEMENT_DESC{ "THICK", 1, DXGI_FORMAT_R32_FLOAT, 0, 68, ff_dx::D3D_IPVA, 0 },
        ff_dx::D3D_INPUT_ELEMENT_DESC{ "DEPTH", 0, DXGI_FORMAT_R32_FLOAT, 0, 72, ff_dx::D3D_IPVA, 0 },
        ff_dx::D3D_INPUT_ELEMENT_DESC{ "MATRIX", 0, DXGI_FORMAT_R32_UINT, 0, 76, ff_dx::D3D_IPVA, 0 },
    };

    return layout;
}

const std::array<ff_dx::D3D_INPUT_ELEMENT_DESC, 6>& ff::vertex::circle_geometry::layout()
{
    static const std::array<ff_dx::D3D_INPUT_ELEMENT_DESC, 6> layout
    {
        ff_dx::D3D_INPUT_ELEMENT_DESC{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, ff_dx::D3D_IPVA, 0 },
        ff_dx::D3D_INPUT_ELEMENT_DESC{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, ff_dx::D3D_IPVA, 0 },
        ff_dx::D3D_INPUT_ELEMENT_DESC{ "COLOR", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 28, ff_dx::D3D_IPVA, 0 },
        ff_dx::D3D_INPUT_ELEMENT_DESC{ "RADIUS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 44, ff_dx::D3D_IPVA, 0 },
        ff_dx::D3D_INPUT_ELEMENT_DESC{ "THICK", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 48, ff_dx::D3D_IPVA, 0 },
        ff_dx::D3D_INPUT_ELEMENT_DESC{ "MATRIX", 0, DXGI_FORMAT_R32_UINT, 0, 52, ff_dx::D3D_IPVA, 0 },
    };

    return layout;
}

const std::array<ff_dx::D3D_INPUT_ELEMENT_DESC, 8>& ff::vertex::triangle_geometry::layout()
{
    static const std::array<ff_dx::D3D_INPUT_ELEMENT_DESC, 8> layout
    {
        ff_dx::D3D_INPUT_ELEMENT_DESC{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, ff_dx::D3D_IPVA, 0 },
        ff_dx::D3D_INPUT_ELEMENT_DESC{ "POSITION", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 8, ff_dx::D3D_IPVA, 0 },
        ff_dx::D3D_INPUT_ELEMENT_DESC{ "POSITION", 2, DXGI_FORMAT_R32G32_FLOAT, 0, 16, ff_dx::D3D_IPVA, 0 },
        ff_dx::D3D_INPUT_ELEMENT_DESC{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, ff_dx::D3D_IPVA, 0 },
        ff_dx::D3D_INPUT_ELEMENT_DESC{ "COLOR", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 40, ff_dx::D3D_IPVA, 0 },
        ff_dx::D3D_INPUT_ELEMENT_DESC{ "COLOR", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 56, ff_dx::D3D_IPVA, 0 },
        ff_dx::D3D_INPUT_ELEMENT_DESC{ "DEPTH", 0, DXGI_FORMAT_R32_FLOAT, 0, 72, ff_dx::D3D_IPVA, 0 },
        ff_dx::D3D_INPUT_ELEMENT_DESC{ "MATRIX", 0, DXGI_FORMAT_R32_UINT, 0, 76, ff_dx::D3D_IPVA, 0 },
    };

    return layout;
}

const std::array<ff_dx::D3D_INPUT_ELEMENT_DESC, 8>& ff::vertex::sprite_geometry::layout()
{
    static const std::array<ff_dx::D3D_INPUT_ELEMENT_DESC, 8> layout
    {
        ff_dx::D3D_INPUT_ELEMENT_DESC{ "RECT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, ff_dx::D3D_IPVA, 0 },
        ff_dx::D3D_INPUT_ELEMENT_DESC{ "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, ff_dx::D3D_IPVA, 0 },
        ff_dx::D3D_INPUT_ELEMENT_DESC{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, ff_dx::D3D_IPVA, 0 },
        ff_dx::D3D_INPUT_ELEMENT_DESC{ "SCALE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 48, ff_dx::D3D_IPVA, 0 },
        ff_dx::D3D_INPUT_ELEMENT_DESC{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 56, ff_dx::D3D_IPVA, 0 },
        ff_dx::D3D_INPUT_ELEMENT_DESC{ "ROTATE", 0, DXGI_FORMAT_R32_FLOAT, 0, 68, ff_dx::D3D_IPVA, 0 },
        ff_dx::D3D_INPUT_ELEMENT_DESC{ "TEXINDEX", 0, DXGI_FORMAT_R32_UINT, 0, 72, ff_dx::D3D_IPVA, 0 },
        ff_dx::D3D_INPUT_ELEMENT_DESC{ "MATRIX", 0, DXGI_FORMAT_R32_UINT, 0, 76, ff_dx::D3D_IPVA, 0 },
    };

    return layout;
}
