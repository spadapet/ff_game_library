#pragma once

#include "../dxgi/vertex.h"

/// <summary>
/// Used by ff::dx12::draw_device for rendering 2D graphics
/// </summary>
namespace ff::dx12::vertex
{
    struct line_geometry : public ff::dxgi::vertex::line_geometry
    {
        static const std::array<D3D12_INPUT_ELEMENT_DESC, 10>& layout();
    };

    struct circle_geometry : public ff::dxgi::vertex::circle_geometry
    {
        static const std::array<D3D12_INPUT_ELEMENT_DESC, 6>& layout();
    };

    struct triangle_geometry : public ff::dxgi::vertex::triangle_geometry
    {
        static const std::array<D3D12_INPUT_ELEMENT_DESC, 8>& layout();
    };

    struct sprite_geometry : public ff::dxgi::vertex::sprite_geometry
    {
        static const std::array<D3D12_INPUT_ELEMENT_DESC, 8>& layout();
    };
}
