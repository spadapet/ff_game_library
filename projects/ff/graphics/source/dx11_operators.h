#pragma once

#if DXVER == 11


inline bool operator==(const D3D11_BLEND_DESC& lhs, const D3D11_BLEND_DESC& rhs)
{
    return std::memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
}

inline bool operator==(const D3D11_DEPTH_STENCIL_DESC& lhs, const D3D11_DEPTH_STENCIL_DESC& rhs)
{
    return std::memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
}

inline bool operator==(const D3D11_RASTERIZER_DESC& lhs, const D3D11_RASTERIZER_DESC& rhs)
{
    return std::memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
}

inline bool operator==(const D3D11_SAMPLER_DESC& lhs, const D3D11_SAMPLER_DESC& rhs)
{
    return std::memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
}

#endif
