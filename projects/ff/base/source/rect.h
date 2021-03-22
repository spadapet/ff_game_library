#pragma once

#include "point.h"

namespace ff
{
    template<class T>
    struct rect_t
    {
        T left, top, right, bottom;

        using this_type = rect_t<T>;
        using point_type = point_t<T>;
        using data_type = T;

        static constexpr T value_zero = T{};
        static constexpr T value_two = static_cast<T>(2);

        rect_t() = default;

        rect_t(const this_type& other) = default;

        rect_t(T left, T top, T right, T bottom)
            : left(left), top(top), right(right), bottom(bottom)
        {}

        rect_t(const point_type& top_left, const point_type& bottom_right)
            : left(top_left.x), top(top_left.y), right(bottom_right.x), bottom(bottom_right.y)
        {}

        template<class T2>
        rect_t<T2> cast() const
        {
            return rect_t<T2>(static_cast<T2>(this->left), static_cast<T2>(this->top), static_cast<T2>(this->right), static_cast<T2>(this->bottom));
        }

        operator bool() const
        {
            return this->left != this_type::value_zero || this->top != this_type::value_zero || this->right != this_type::value_zero || this->bottom != this_type::value_zero;
        }

        bool operator!() const
        {
            return this->left == this_type::value_zero && this->top == this_type::value_zero && this->right == this_type::value_zero && this->bottom == this_type::value_zero;
        }

        this_type& operator=(const this_type& other) = default;

        bool operator==(const this_type& other) const
        {
            return this->left == other.left && this->top == other.top && this->right == other.right && this->bottom == other.bottom;
        }

        bool operator!=(const this_type& other) const
        {
            return this->left != other.left || this->top != other.top || this->right != other.right || this->bottom != other.bottom;
        }

        this_type& operator+=(const point_type& value)
        {
            this->left += value.x;
            this->top += value.y;
            this->right += value.x;
            this->bottom += value.y;
            return *this;
        }

        this_type& operator+=(T value)
        {
            this->left += value;
            this->top += value;
            this->right += value;
            this->bottom += value;
            return *this;
        }

        this_type& operator-=(const point_type& value)
        {
            this->left -= value.x;
            this->top -= value.y;
            this->right -= value.x;
            this->bottom -= value.y;
            return *this;
        }

        this_type& operator-=(T value)
        {
            this->left -= value;
            this->top -= value;
            this->right -= value;
            this->bottom -= value;
            return *this;
        }

        this_type& operator*=(const point_type& value)
        {
            this->left *= value.x;
            this->top *= value.y;
            this->right *= value.x;
            this->bottom *= value.y;
            return *this;
        }

        this_type& operator*=(T value)
        {
            this->left *= value;
            this->top *= value;
            this->right *= value;
            this->bottom *= value;
            return *this;
        }

        this_type& operator/=(const point_type& value)
        {
            this->left /= value.x;
            this->top /= value.y;
            this->right /= value.x;
            this->bottom /= value.y;
            return *this;
        }

        this_type& operator/=(T value)
        {
            this->left /= value;
            this->top /= value;
            this->right /= value;
            this->bottom /= value;
            return *this;
        }

        this_type operator+(const point_type& point) const
        {
            return this_type(this->left + point.x, this->top + point.y, this->right + point.x, this->bottom + point.y);
        }

        this_type operator+(T value) const
        {
            return this_type(this->left + value, this->top + value, this->right + value, this->bottom + value);
        }

        this_type operator-(const point_type& point) const
        {
            return this_type(this->left - point.x, this->top - point.y, this->right - point.x, this->bottom - point.y);
        }

        this_type operator-(T value) const
        {
            return this_type(this->left - value, this->top - value, this->right - value, this->bottom - value);
        }

        this_type operator*(const point_type& value) const
        {
            return this_type(this->left * value.x, this->top * value.y, this->right * value.x, this->bottom * value.y);
        }

        this_type operator*(T value) const
        {
            return this_type(this->left * value, this->top * value, this->right * value, this->bottom * value);
        }

        this_type operator/(const point_type& value) const
        {
            return this_type(this->left / value.x, this->top / value.y, this->right / value.x, this->bottom / value.y);
        }

        this_type operator/(T value) const
        {
            return this_type(this->left / value, this->top / value, this->right / value, this->bottom / value);
        }

        T width() const
        {
            return this->right - this->left;
        }

        T height() const
        {
            return this->bottom - this->top;
        }

        T area() const
        {
            return this->width() * this->height();
        }

        T center_x() const
        {
            return (this->left + this->right) / this_type::value_two;
        }

        T center_y() const
        {
            return (this->top + this->bottom) / this_type::value_two;
        }

        point_type size() const
        {
            return point_type(this->right - this->left, this->bottom - this->top);
        }

        point_type center() const
        {
            return point_type(this->center_x(), this->center_y());
        }

        bool empty() const
        {
            return this->bottom == this->top && this->right == this->left;
        }

        bool contains(const point_type& point) const
        {
            return point.x >= this->left && point.x < this->right&& point.y >= this->top && point.y < this->bottom;
        }

        bool touches(const point_type& point) const
        {
            return point.x >= this->left && point.x <= this->right && point.y >= this->top && point.y <= this->bottom;
        }

        bool touches(const this_type& other) const
        {
            return this->right >= other.left && this->left <= other.right && this->bottom >= other.top && this->top <= other.bottom;
        }

        bool touches_hoiz_intersects_vert(const this_type& other) const
        {
            return this->right >= other.left && this->left <= other.right && this->bottom > other.top && this->top < other.bottom;
        }

        bool touches_vert_intersects_horiz(const this_type& other) const
        {
            return this->right > other.left && this->left < other.right&& this->bottom >= other.top && this->top <= other.bottom;
        }

        bool intersects(const this_type& other) const
        {
            return this->right > other.left && this->left < other.right&& this->bottom > other.top && this->top < other.bottom;
        }

        this_type intersection(const this_type& other) const
        {
            this_type rect(
                std::max(this->left, other.left),
                std::max(this->top, other.top),
                std::min(this->right, other.right),
                std::min(this->bottom, other.bottom));

            if (rect.left > rect.right || rect.top > rect.bottom)
            {
                return this_type{};
            }

            return rect;
        }

        bool inside(const this_type& other) const
        {
            return this->left >= other.left && this->right <= other.right && this->top >= other.top && this->bottom <= other.bottom;
        }

        bool outside(const this_type& other) const
        {
            return this->left >= other.right || this->right <= other.left || this->top >= other.bottom || this->bottom <= other.top;
        }

        point_type top_left() const
        {
            return point_type(this->left, this->top);
        }

        point_type top_right() const
        {
            return point_type(this->right, this->top);
        }

        point_type bottom_left() const
        {
            return point_type(this->left, this->bottom);
        }

        point_type bottom_right() const
        {
            return point_type(this->right, this->bottom);
        }

        this_type left_edge() const
        {
            return this_type(this->left, this->top, this->left, this->bottom);
        }

        this_type top_edge() const
        {
            return this_type(this->left, this->top, this->right, this->top);
        }

        this_type right_edge() const
        {
            return this_type(this->right, this->top, this->right, this->bottom);
        }

        this_type bottom_edge() const
        {
            return this_type(this->left, this->bottom, this->right, this->bottom);
        }

        std::array<point_type, 4> corners() const
        {
            std::array<point_type, 4> corners =
            {
                this->top_left(),
                this->top_right(),
                this->bottom_right(),
                this->bottom_left(),
            };

            return corners;
        }

        this_type offset(T x, T y) const
        {
            return this_type(this->left + x, this->top + y, this->right + x, this->bottom + y);
        }

        this_type offset(const point_type& value) const
        {
            return this_type(this->left + value.x, this->top + value.y, this->right + value.x, this->bottom + value.y);
        }

        this_type offset_size(T x, T y) const
        {
            return this_type(this->left, this->top, this->right + x, this->bottom + y);
        }

        this_type offset_size(const point_type& value) const
        {
            return this_type(this->left, this->top, this->right + value.x, this->bottom + value.y);
        }

        this_type normalize() const
        {
            this_type rect = *this;

            if (rect.left > rect.right)
            {
                std::swap(rect.left, rect.right);
            }

            if (this->top > this->bottom)
            {
                std::swap(rect.top, rect.bottom);
            }

            return rect;
        }

        this_type deflate(T x, T y) const
        {
            return this->deflate(x, y, x, y);
        }

        this_type deflate(T x, T y, T x2, T y2) const
        {
            return this_type(this->left + x, this->top + y, this->right - x2, this->bottom - y2).normalize();
        }

        this_type deflate(const point_type& corners) const
        {
            return this->deflate(corners.x, corners.y, corners.x, corners.y);
        }

        this_type deflate(const point_type& top_left, const point_type& bottom_right) const
        {
            return this->deflate(top_left.x, top_left.y, bottom_right.x, bottom_right.y);
        }

        this_type inflate(T x, T y) const
        {
            return this->inflate(x, y, x, y);
        }

        this_type inflate(T x, T y, T x2, T y2) const
        {
            return this_type(this->left - x, this->top - y, this->right + x2, this->bottom + y2).normalize();
        }

        this_type inflate(const point_type& corners) const
        {
            return this->inflate(corners.x, corners.y, corners.x, corners.y);
        }

        this_type inflate(const point_type& top_left, const point_type& bottom_right) const
        {
            return this->inflate(top_left.x, top_left.y, bottom_right.x, bottom_right.y);
        }

        this_type boundary(const this_type& other) const
        {
            return this_type(
                std::min(this->left, other.left),
                std::min(this->top, other.top),
                std::max(this->right, other.right),
                std::max(this->bottom, other.bottom));
        }

        this_type center(const this_type& other) const
        {
            T left2 = other.left + (other.width() - this->width()) / this_type::value_two;
            T top2 = other.top + (other.height() - this->height()) / this_type::value_two;

            return this_type(left2, top2, left2 + this->width(), top2 + this->height());
        }

        this_type center(const point_type& point) const
        {
            T left2 = point.x - this->width() / this_type::value_two;
            T top2 = point.y - this->height() / this_type::value_two;

            return this_type(left2, top2, left2 + this->width(), top2 + this->height());
        }

        this_type move_inside(const this_type& other) const
        {
            this_type rect = *this;

            if (rect.left < other.left)
            {
                T offset = other.left - rect.left;

                rect.left += offset;
                rect.right += offset;
            }
            else if (rect.right > other.right)
            {
                T offset = rect.right - other.right;

                rect.left -= offset;
                rect.right -= offset;
            }

            if (rect.top < other.top)
            {
                T offset = other.top - rect.top;

                rect.top += offset;
                rect.bottom += offset;
            }
            else if (rect.bottom > other.bottom)
            {
                T offset = rect.bottom - other.bottom;

                rect.top -= offset;
                rect.bottom -= offset;
            }

            return rect;
        }

        this_type move_outside(const this_type& other) const
        {
            this_type rect = *this;

            if (this->intersects(other))
            {
                T left_move = this->right - other.left;
                T right_move = other.right - this->left;
                T top_move = this->bottom - other.top;
                T bottom_move = other.bottom - this->top;
                T move = std::min({ left_move, right_move, top_move, bottom_move });

                if (move == left_move)
                {
                    rect = rect.offset(-left_move, this_type::value_zero);
                }

                if (move == top_move)
                {
                    rect = rect.offset(this_type::value_zero, -top_move);
                }

                if (move == right_move)
                {
                    rect = rect.offset(right_move, this_type::value_zero);
                }

                if (move == bottom_move)
                {
                    rect = rect.offset(this_type::value_zero, bottom_move);
                }
            }

            return rect;
        }

        this_type move_top_left(const point_type& value) const
        {
            this->offset(value.x - this->left, value.y - this->top);
        }

        this_type move_top_left(T x, T y) const
        {
            this->offset(x - this->left, y - this->top);
        }

        this_type crop(const this_type& other) const
        {
            this_type rect = *this;

            if (rect.left < other.left)
            {
                rect.left = other.left;
            }

            if (rect.right > other.right)
            {
                rect.right = other.right;
            }

            if (rect.top < other.top)
            {
                rect.top = other.top;
            }

            if (rect.bottom > other.bottom)
            {
                rect.bottom = other.bottom;
            }

            return rect;
        }

        this_type scale_to_fit(const this_type& other) const
        {
            this_type rect = *this;

            if (rect.left == rect.right)
            {
                if (rect.top != rect.bottom)
                {
                    rect.bottom = rect.top + other.height();
                }
            }
            else if (rect.top == rect.bottom)
            {
                rect.right = rect.left + other.width();
            }
            else
            {
                rect_t<double> this_d = this->cast<double>();
                rect_t<double> other_d = other.cast<double>();
                double ratio = this_d.width() / this_d.height();

                if (other_d.width() / ratio > other_d.height())
                {
                    rect.right = rect.left + static_cast<T>(other_d.height() * ratio);
                    rect.bottom = rect.top + other.height();
                }
                else
                {
                    rect.right = rect.left + other.width();
                    rect.bottom = rect.top + static_cast<T>(other_d.width() / ratio);
                }
            }

            return rect;
        }

        this_type interpolate(const this_type& other, double value) const
        {
            rect_t<double> dr1 = this->cast<double>();
            rect_t<double> dr2 = other.cast<double>();

            return rect_t<double>(
                (dr2.left - dr1.left) * value + dr1.left,
                (dr2.top - dr1.top) * value + dr1.top,
                (dr2.right - dr1.right) * value + dr1.right,
                (dr2.bottom - dr1.bottom) * value + dr1.bottom).cast<T>();
        }
    };

    using rect_int = rect_t<int>;
    using rect_short = rect_t<short>;
    using rect_float = rect_t<float>;
    using rect_double = rect_t<double>;
    using rect_size = rect_t<size_t>;
    using rect_fixed = rect_t<ff::fixed_int>;
}

namespace std
{
    template<class T>
    static ff::rect_t<T> floor(const ff::rect_t<T>& value)
    {
        return ff::rect_t<T>(std::floor(value.left), std::floor(value.top), std::floor(value.right), std::floor(value.bottom));
    }
}
