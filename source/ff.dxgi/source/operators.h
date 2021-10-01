#pragma once

inline bool operator==(const DirectX::XMVECTOR& lhs, const DirectX::XMVECTOR& rhs)
{
    return DirectX::XMVector4Equal(lhs, rhs);
}

inline bool operator!=(const DirectX::XMVECTOR& lhs, const DirectX::XMVECTOR& rhs)
{
    return DirectX::XMVector4NotEqual(lhs, rhs);
}

inline bool operator==(const DirectX::XMMATRIX& lhs, const DirectX::XMMATRIX& rhs)
{
    return DirectX::XMVector4Equal(lhs.r[0], rhs.r[0]) &&
        DirectX::XMVector4Equal(lhs.r[1], rhs.r[1]) &&
        DirectX::XMVector4Equal(lhs.r[2], rhs.r[2]) &&
        DirectX::XMVector4Equal(lhs.r[3], rhs.r[3]);
}

inline bool operator!=(const DirectX::XMMATRIX& lhs, const DirectX::XMMATRIX& rhs)
{
    return DirectX::XMVector4NotEqual(lhs.r[0], rhs.r[0]) ||
        DirectX::XMVector4NotEqual(lhs.r[1], rhs.r[1]) ||
        DirectX::XMVector4NotEqual(lhs.r[2], rhs.r[2]) ||
        DirectX::XMVector4NotEqual(lhs.r[3], rhs.r[3]);
}

inline bool operator==(const DirectX::XMFLOAT4& lhs, const DirectX::XMFLOAT4& rhs)
{
    return std::memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
}

inline bool operator!=(const DirectX::XMFLOAT4& lhs, const DirectX::XMFLOAT4& rhs)
{
    return std::memcmp(&lhs, &rhs, sizeof(lhs)) != 0;
}

inline bool operator==(const DirectX::XMFLOAT4X4& lhs, const DirectX::XMFLOAT4X4& rhs)
{
    return std::memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
}

inline bool operator!=(const DirectX::XMFLOAT4X4& lhs, const DirectX::XMFLOAT4X4& rhs)
{
    return std::memcmp(&lhs, &rhs, sizeof(lhs)) != 0;
}
