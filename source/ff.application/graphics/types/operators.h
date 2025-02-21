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

namespace ff::dxgi
{
    inline const DirectX::XMFLOAT4& cast_rect(const ff::rect_float& rect)
    {
        return reinterpret_cast<const DirectX::XMFLOAT4&>(rect);
    }

    inline const ff::rect_float& cast_rect(const DirectX::XMFLOAT4& rect)
    {
        return reinterpret_cast<const ff::rect_float&>(rect);
    }

    inline const DirectX::XMFLOAT2& cast_point(const ff::point_float& point)
    {
        return reinterpret_cast<const DirectX::XMFLOAT2&>(point);
    }

    inline const ff::point_float& cast_point(const DirectX::XMFLOAT2& point)
    {
        return reinterpret_cast<const ff::point_float&>(point);
    }
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
