#pragma once

#include "value_type_base.h"
#include "value_vector_base.h"

namespace ff::type
{
    class string_v : public ff::value
    {
    public:
        string_v(std::string&& value);
        string_v(const std::string& value);
        ~string_v();

        const std::string& get() const;
        static ff::value* get_static_value(const std::string& value);
        static ff::value* get_static_default_value();

    private:
        std::string value;
    };

    template<>
    struct value_traits<std::string> : public value_derived_traits<ff::type::string_v>
    {};

    class string_type : public ff::internal::value_type_simple<string_v>
    {
    public:
        using value_type_simple::value_type_simple;

        virtual value_ptr try_convert_to(const value* val, std::type_index type) const;
    };

    using string_vector = ff::internal::value_vector_base<std::string>;

    template<>
    struct value_traits<std::vector<std::string>> : public value_derived_traits<ff::type::string_vector>
    {};

    class string_vector_type : public ff::internal::value_type_object_vector<ff::type::string_vector>
    {
        using value_type_object_vector::value_type_object_vector;
    };
}
