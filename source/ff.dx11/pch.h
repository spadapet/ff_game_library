#pragma once

// C++
#include <unordered_map>
#include <unordered_set>

// Windows
#include <d3d11_4.h>

// FF
#include <ff.dxgi.h>
#include <ff.resource.h>

// DirectX interface usage

using ID3D11DeviceX = typename ID3D11Device5;
using ID3D11DeviceContextX = typename ID3D11DeviceContext4;

namespace ff::dx11
{
    using D3D_INPUT_ELEMENT_DESC = typename D3D11_INPUT_ELEMENT_DESC;
    static const D3D11_INPUT_CLASSIFICATION D3D_IPVA = D3D11_INPUT_PER_VERTEX_DATA;
}
