#pragma once

#include "../value/value.h"
#include "../value/value_type_base.h"

namespace ff::type
{
    class string : public ff::value
    {
    public:
        string(std::string&& value);
        string(const std::string& value);

        const std::string& get() const;
        static ff::value* get_static_value(std::string&& value);
        static ff::value* get_static_value(const std::string& value);
        static ff::value* get_static_default_value();

    private:
        std::string value;
    };

    template<>
    struct value_traits<std::string> : public value_derived_traits<ff::type::string>
    {};

    class string_type : public ff::value_type_base<string>
    {
    public:
        //virtual value_ptr try_convert_to(const value* val, std::type_index type) const;
    };
}
