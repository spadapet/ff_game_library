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

        static constexpr T value_zero = static_cast<T>(0);
        static constexpr T value_one = static_cast<T>(1);

        point_t() = default;

        point_t(const this_type& other) = default;

        point_t(T x, T y)
            : x(x), y(y)
        {
        }

        static this_type zeros()
        {
            return this_type(this_type::value_zero, this_type::value_zero);
        }

        static this_type ones()
        {
            return this_type(this_type::value_one, this_type::value_one);
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

        this_type operator-()
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
    using point_fixed = point_t<ff::int32_fixed8_t>;
}
