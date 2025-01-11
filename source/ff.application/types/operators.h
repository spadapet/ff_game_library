#pragma once

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

namespace std
{
    template<>
    struct equal_to<DirectX::XMFLOAT4>
    {
        inline bool operator()(const DirectX::XMFLOAT4& lhs, const DirectX::XMFLOAT4& rhs) const
        {
            return std::memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
        }
    };

    template<>
    struct equal_to<DirectX::XMFLOAT4X4>
    {
        inline bool operator()(const DirectX::XMFLOAT4X4& lhs, const DirectX::XMFLOAT4X4& rhs) const
        {
            return std::memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
        }
    };
}
