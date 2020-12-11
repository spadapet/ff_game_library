#pragma once

#include "value.h"
#include "value_type_base.h"

namespace ff::type
{
    class null_v : public ff::value
    {
    public:
        std::nullptr_t get() const;
        static ff::value* get_static_value();
        static ff::value* get_static_default_value();
    };

    template<>
    struct value_traits<std::nullptr_t> : public value_derived_traits<ff::type::null_v>
    {};

    class null_type : public ff::value_type_base<ff::type::null_v>
    {
    public:
        using value_type_base::value_type_base;

        virtual value_ptr try_convert_to(const value* val, std::type_index type) const;
        virtual value_ptr load(reader_base& reader) const override;
        virtual bool save(const value* val, writer_base& writer) const override;
    };
}
