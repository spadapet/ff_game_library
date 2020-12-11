#pragma once

#include "string_v.h"
#include "value.h"
#include "value_type_base.h"

namespace ff::type
{
    template<class T>
    class rect_base_v : public ff::value
    {
    public:
        using this_type = typename rect_base_v<T>;
        using rect_type = typename T;
        using data_type = typename T::data_type;

        rect_base_v(const rect_type& value)
            : value(value)
        {}

        const rect_type& get() const
        {
            return this->value;
        }

        static ff::value* get_static_value(const rect_type& value)
        {
            return value == rect_type::zeros() ? this_type::get_static_default_value() : nullptr;
        }

        static ff::value* get_static_default_value()
        {
            static this_type default_value = rect_type::zeros();
            return &default_value;
        }

    private:
        rect_type value;
    };

    using rect_double_v = typename rect_base_v<ff::rect_double>;
    using rect_fixed_v = typename rect_base_v<ff::rect_fixed>;
    using rect_float_v = typename rect_base_v<ff::rect_float>;
    using rect_int_v = typename rect_base_v<ff::rect_int>;
    using rect_size_v = typename rect_base_v<ff::rect_size>;

    template<>
    struct value_traits<ff::rect_double> : public value_derived_traits<ff::type::rect_double_v> 
    {};

    template<>
    struct value_traits<ff::rect_fixed> : public value_derived_traits<ff::type::rect_fixed_v>
   {};

    template<>
    struct value_traits<ff::rect_float> : public value_derived_traits<ff::type::rect_float_v>
    {};

    template<>
    struct value_traits<ff::rect_int> : public value_derived_traits<ff::type::rect_int_v>
    {};

    template<>
    struct value_traits<ff::rect_size> : public value_derived_traits<ff::type::rect_size_v>
    {};

    template<class T>
    class rect_base_type : public ff::internal::value_type_simple<T>
    {
    public:
        using value_type = typename T;
        using rect_type = typename T::rect_type;
        using data_type = typename T::data_type;

        using value_type_simple::value_type_simple;

        virtual value_ptr try_convert_to(const value* val, std::type_index type) const override
        {
            if (type == typeid(ff::type::string_v))
            {
                const rect_type& src = val->get<rect_type>();
                std::ostringstream str;
                str << "[" << src.left << ", " << src.top << ", " << src.right << ", " << src.bottom << "]";
                return ff::value::create<std::string>(str.str());
            }

            return nullptr;
        }

        virtual value_ptr try_convert_from(const value* val) const override
        {
            if (val->can_have_indexed_children() && val->index_child_count() == 4)
            {
                ff::value_ptr v0 = val->index_child(0)->try_convert<data_type>();
                ff::value_ptr v1 = val->index_child(1)->try_convert<data_type>();
                ff::value_ptr v2 = val->index_child(2)->try_convert<data_type>();
                ff::value_ptr v3 = val->index_child(3)->try_convert<data_type>();

                if (v0 && v1 && v2 && v3)
                {
                    return ff::value::create<rect_type>(rect_type(
                        v0->get<data_type>(),
                        v1->get<data_type>(),
                        v2->get<data_type>(),
                        v3->get<data_type>()));
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
                    return ff::value::create<data_type>(val->get<rect_type>().left);

                case 1:
                    return ff::value::create<data_type>(val->get<rect_type>().top);

                case 2:
                    return ff::value::create<data_type>(val->get<rect_type>().right);

                case 3:
                    return ff::value::create<data_type>(val->get<rect_type>().bottom);

                default:
                    return nullptr;
            }
        }

        virtual size_t index_child_count(const value* val) const
        {
            return 4;
        }
    };

    using rect_double_type = typename rect_base_type<rect_double_v>;
    using rect_fixed_type = typename rect_base_type<rect_fixed_v>;
    using rect_float_type = typename rect_base_type<rect_float_v>;
    using rect_int_type = typename rect_base_type<rect_int_v>;
    using rect_size_type = typename rect_base_type<rect_size_v>;
}
