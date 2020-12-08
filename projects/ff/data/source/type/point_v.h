#pragma once

#include "string_v.h"
#include "../value/value.h"
#include "../value/value_type_base.h"

namespace ff::type
{
    template<class T>
    class point_base_v : public ff::value
    {
    public:
        using this_type = typename point_base_v<T>;
        using point_type = typename T;
        using data_type = typename T::data_type;

        point_base_v(const point_type& value)
            : value(value)
        {}

        const point_type& get() const
        {
            return this->value;
        }

        static ff::value* get_static_value(const point_type& value)
        {
            return value == point_type::zeros() ? this_type::get_static_default_value() : nullptr;
        }

        static ff::value* get_static_default_value()
        {
            static this_type default_value = point_type::zeros();
            return &default_value;
        }

    private:
        point_type value;
    };

    using point_double_v = typename point_base_v<ff::point_double>;
    using point_fixed_v = typename point_base_v<ff::point_fixed>;
    using point_float_v = typename point_base_v<ff::point_float>;
    using point_int_v = typename point_base_v<ff::point_int>;
    using point_size_v = typename point_base_v<ff::point_size>;

    template<>
    struct value_traits<ff::point_double> : public value_derived_traits<ff::type::point_double_v> 
    {};

    template<>
    struct value_traits<ff::point_fixed> : public value_derived_traits<ff::type::point_fixed_v>
   {};

    template<>
    struct value_traits<ff::point_float> : public value_derived_traits<ff::type::point_float_v>
    {};

    template<>
    struct value_traits<ff::point_int> : public value_derived_traits<ff::type::point_int_v>
    {};

    template<>
    struct value_traits<ff::point_size> : public value_derived_traits<ff::type::point_size_v>
    {};

    template<class T>
    class point_base_type : public ff::value_type_simple<T>
    {
    public:
        using value_type = typename T;
        using point_type = typename T::point_type;
        using data_type = typename T::data_type;

        using value_type_simple::value_type_simple;

        virtual value_ptr try_convert_to(const value* val, std::type_index type) const override
        {
            if (type == typeid(ff::type::string_v))
            {
                const point_type& src = val->get<point_type>();
                std::stringstream str;
                str << "[" << src.x << ", " << src.y << "]";
                return ff::value::create<std::string>(str.str());
            }

            return nullptr;
        }

        virtual value_ptr try_convert_from(const value* val) const override
        {
            if (val->can_have_indexed_children() && val->index_child_count() == 2)
            {
                ff::value_ptr v0 = val->index_child(0)->try_convert<data_type>();
                ff::value_ptr v1 = val->index_child(1)->try_convert<data_type>();

                if (v0 && v1)
                {
                    return ff::value::create<point_type>(point_type(v0->get<data_type>(), v1->get<data_type>()));
                }
            }

            return nullptr;
        }

        virtual bool can_have_indexed_children() const
        {
            return true;
        }

        virtual value_ptr index_child(const value* val, size_t index) const
        {
            switch (index)
            {
                case 0:
                    return ff::value::create<data_type>(val->get<point_type>().x);

                case 1:
                    return ff::value::create<data_type>(val->get<point_type>().y);

                default:
                    return nullptr;
            }
        }

        virtual size_t index_child_count(const value* val) const
        {
            return 2;
        }
    };

    using point_double_type = typename point_base_type<point_double_v>;
    using point_fixed_type = typename point_base_type<point_fixed_v>;
    using point_float_type = typename point_base_type<point_float_v>;
    using point_int_type = typename point_base_type<point_int_v>;
    using point_size_type = typename point_base_type<point_size_v>;
}
