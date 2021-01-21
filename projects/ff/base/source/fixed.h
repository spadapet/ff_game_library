#pragma once

namespace ff
{
    template<class T, class ExpandedT, T FixedCount>
    class fixed_t
    {
    public:
        using this_type = typename fixed_t<T, ExpandedT, FixedCount>;
        using data_type = typename T;
        using expanded_type = typename ExpandedT;

        static const T fixed_zero = static_cast<T>(0);
        static const T fixed_count = FixedCount;
        static const T fixed_max = (1 << fixed_count);
        static const T fraction_mask = fixed_max - 1;
        static const T whole_mask = ~fraction_mask;

        fixed_t() = default;

        fixed_t(const fixed_t& rhs) = default;

        constexpr fixed_t(int data)
            : data(data << this_type::fixed_count)
        {}

        constexpr fixed_t(float data)
            : data(static_cast<T>(data* this_type::fixed_max))
        {}

        constexpr fixed_t(double data)
            : data(static_cast<T>(data* this_type::fixed_max))
        {}

        constexpr fixed_t(bool data)
            : fixed_t(static_cast<int>(data))
        {}

        operator T() const
        {
            return this->data >> this_type::fixed_count;
        }

        operator float() const
        {
            return static_cast<float>(static_cast<double>(*this));
        }

        operator double() const
        {
            return static_cast<double>(this->data) / this_type::fixed_max;
        }

        operator bool() const
        {
            return this->data != this_type::fixed_zero;
        }

        fixed_t& operator=(const fixed_t& rhs) = default;

        bool operator!() const
        {
            return this->data == this_type::fixed_zero;
        }

        bool operator==(const this_type& rhs) const
        {
            return this->data == rhs.data;
        }

        bool operator!=(const this_type& rhs) const
        {
            return this->data != rhs.data;
        }

        bool operator<(const this_type& rhs) const
        {
            return this->data < rhs.data;
        }

        bool operator>(const this_type& rhs) const
        {
            return this->data > rhs.data;
        }

        bool operator<=(const this_type& rhs) const
        {
            return this->data <= rhs.data;
        }

        bool operator>=(const this_type& rhs) const
        {
            return this->data >= rhs.data;
        }

        this_type operator-() const
        {
            return this_type::from_raw(-this->data);
        }

        this_type operator+() const
        {
            return this_type::from_raw(+this->data);
        }

        this_type operator++()
        {
            return *this = (*this + this_type(1));
        }

        this_type operator++(int after)
        {
            this_type orig = *this;
            *this = (*this + this_type(1));
            return orig;
        }

        this_type operator--()
        {
            return *this = (*this - this_type(1));
        }

        this_type operator--(int after)
        {
            this_type orig = *this;
            *this = (*this - this_type(1));
            return orig;
        }

        this_type& operator+=(const this_type& rhs)
        {
            return *this = (*this + rhs);
        }

        this_type& operator-=(const this_type& rhs)
        {
            return *this = (*this - rhs);
        }

        this_type& operator*=(const this_type& rhs)
        {
            return *this = (*this * rhs);
        }

        this_type& operator/=(const this_type& rhs)
        {
            return *this = (*this / rhs);
        }

        this_type operator+(const this_type& rhs) const
        {
            return this_type::from_raw(this->data + rhs.data);
        }

        this_type operator-(const this_type& rhs) const
        {
            return this_type::from_raw(this->data - rhs.data);
        }

        this_type operator*(const this_type& rhs) const
        {
            return this_type::from_expanded((this->get_expanded() * rhs.get_expanded()) >> this_type::fixed_count);
        }

        this_type operator/(const this_type& rhs) const
        {
            return this_type::from_expanded((this->get_expanded() << this_type::fixed_count) / rhs.get_expanded());
        }

        this_type operator%(const this_type& rhs) const
        {
            return this_type::from_raw(this->data % rhs.data);
        }

        this_type& operator*=(int rhs)
        {
            return *this = (*this * rhs);
        }

        this_type& operator/=(int rhs)
        {
            return *this = (*this / rhs);
        }

        this_type operator*(int rhs) const
        {
            return this_type::from_raw(this->data * rhs);
        }

        this_type operator/(int rhs) const
        {
            return this_type::from_raw(this->data / rhs);
        }

        this_type abs() const
        {
            return this_type::from_raw(std::abs(this->data));
        }

        this_type ceil() const
        {
            return this_type::from_raw((this->data & this_type::whole_mask) + ((this->data & this_type::fraction_mask) != 0) * this_type::fixed_max);
        }

        this_type floor() const
        {
            return this_type::from_raw(this->data & this_type::whole_mask);
        }

        this_type trunc() const
        {
            return this_type::from_raw((this->data & this_type::whole_mask) + (this->data < this_type::fixed_zero) * ((this->data & this_type::fraction_mask) != this_type::fixed_zero) * this_type::fixed_max);
        }

        this_type copysign(this_type sign) const
        {
            return sign.data >= this_type::fixed_zero ? this->abs() : -this->abs();
        }

        void swap(this_type& rhs)
        {
            std::swap(this->data, rhs.data);
        }

        static this_type from_raw(T data)
        {
            this_type value;
            value.data = data;
            return value;
        }

        T get_raw() const
        {
            return this->data;
        }

    private:
        static this_type from_expanded(ExpandedT data)
        {
            this_type value;
            value.data = static_cast<T>(data);
            return value;
        }

        ExpandedT get_expanded() const
        {
            return static_cast<int64_t>(this->data);
        }

        T data;
    };

    using i32f8_t = fixed_t<int32_t, int64_t, 8>;
}

namespace std
{
    template<class T, class ET, T FC>
    ff::fixed_t<T, ET, FC> abs(const ff::fixed_t<T, ET, FC>& val)
    {
        return val.abs();
    }

    template<class T, class ET, T FC>
    ff::fixed_t<T, ET, FC> ceil(const ff::fixed_t<T, ET, FC>& val)
    {
        return val.ceil();
    }

    template<class T, class ET, T FC>
    ff::fixed_t<T, ET, FC> floor(const ff::fixed_t<T, ET, FC>& val)
    {
        return val.floor();
    }

    template<class T, class ET, T FC>
    ff::fixed_t<T, ET, FC> trunc(const ff::fixed_t<T, ET, FC>& val)
    {
        return val.trunc();
    }

    template<class T, class ET, T FC>
    void swap(ff::fixed_t<T, ET, FC>& lhs, ff::fixed_t<T, ET, FC>& rhs)
    {
        lhs.swap(rhs);
    }

    template<class T, class ET, T FC>
    ff::fixed_t<T, ET, FC> copysign(const ff::fixed_t<T, ET, FC>& val, const ff::fixed_t<T, ET, FC>& sign)
    {
        return val.copysign(sign);
    }

    template<class T, class ET, T FC>
    ff::fixed_t<T, ET, FC> sqrt(const ff::fixed_t<T, ET, FC>& val)
    {
        return std::sqrt(static_cast<double>(val));
    }

    template<class T, class ET, T FC>
    std::ostream& operator<<(std::ostream& out, const ff::fixed_t<T, ET, FC>& val)
    {
        out << static_cast<double>(val);
        return out;
    }

    template<class T, class ET, T FC>
    std::string to_string(const ff::fixed_t<T, ET, FC>& val)
    {
        return std::to_string(static_cast<double>(val));
    }
}
