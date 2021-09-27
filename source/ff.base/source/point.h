#pragma once

#include "fixed.h"

namespace ff
{
    template<class T>
    struct point_t
    {
        T x, y;

        using this_type = point_t<T>;
        using data_type = T;

        static constexpr T value_zero = T{};

        point_t() = default;

        point_t(const this_type& other) = default;

        point_t(T x, T y)
            : x(x), y(y)
        {}

        this_type swap() const
        {
            return this_type(this->y, this->x);
        }

        this_type abs() const
        {
            return this_type(std::abs(this->x), std::abs(this->y));
        }

        T length_squared() const
        {
            return this->x * this->x + this->y * this->y;
        }

        template<class T2>
        point_t<T2> cast() const
        {
            return point_t<T2>(static_cast<T2>(this->x), static_cast<T2>(this->y));
        }

        operator bool() const
        {
            return this->x != this_type::value_zero || this->y != this_type::value_zero;
        }

        bool operator!() const
        {
            return this->x == this_type::value_zero && this->y == this_type::value_zero;
        }

        this_type& operator=(const this_type& other) = default;

        bool operator==(const this_type& other) const
        {
            return this->x == other.x && this->y == other.y;
        }

        bool operator!=(const this_type& other) const
        {
            return this->x != other.x || this->y != other.y;
        }

        this_type operator+(const this_type& other) const
        {
            return this_type(this->x + other.x, this->y + other.y);
        }

        this_type operator-(const this_type& other) const
        {
            return this_type(this->x - other.x, this->y - other.y);
        }

        this_type operator-() const
        {
            return this_type(-this->x, -this->y);
        }

        this_type& operator+=(const this_type& other)
        {
            this->x += other.x;
            this->y += other.y;
            return *this;
        }

        this_type& operator-=(const this_type& other)
        {
            this->x -= other.x;
            this->y -= other.y;
            return *this;
        }

        this_type& operator*=(T scale)
        {
            this->x *= scale;
            this->y *= scale;
            return *this;
        }

        this_type& operator*=(const this_type& other)
        {
            this->x *= other.x;
            this->y *= other.y;
            return *this;
        }

        this_type& operator/=(T scale)
        {
            this->x /= scale;
            this->y /= scale;
            return *this;
        }

        this_type& operator/=(const this_type& other)
        {
            this->x /= other.x;
            this->y /= other.y;
            return *this;
        }

        this_type operator*(T scale) const
        {
            return this_type(this->x * scale, this->y * scale);
        }

        this_type operator/(T scale) const
        {
            return this_type(this->x / scale, this->y / scale);
        }

        this_type operator*(const this_type& other) const
        {
            return this_type(this->x * other.x, this->y * other.y);
        }

        this_type operator/(const this_type& other) const
        {
            return this_type(this->x / other.x, this->y / other.y);
        }
    };

    using point_int = point_t<int>;
    using point_short = point_t<short>;
    using point_float = point_t<float>;
    using point_double = point_t<double>;
    using point_size = point_t<size_t>;
    using point_fixed = point_t<ff::fixed_int>;
}

namespace std
{
    template<class T>
    static ff::point_t<T> floor(const ff::point_t<T>& value)
    {
        return ff::point_t<T>(std::floor(value.x), std::floor(value.y));
    }

    template<class T>
    double atan2(const ff::point_t<T>& value)
    {
        return value ? std::atan2(static_cast<double>(value.y), static_cast<double>(value.x)) : 0;
    }
}
