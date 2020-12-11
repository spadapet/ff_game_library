#pragma once

#include "value.h"
#include "value_type_base.h"

namespace ff::type
{
    class uuid_v : public ff::value
    {
    public:
        uuid_v(const ff::uuid& value);

        const ff::uuid& get() const;
        static ff::value* get_static_value(const ff::uuid& value);
        static ff::value* get_static_default_value();

    private:
        ff::uuid value;
    };

    template<>
    struct value_traits<ff::uuid> : public value_derived_traits<ff::type::uuid_v>
    {};

    class uuid_type : public ff::value_type_simple<ff::type::uuid_v>
    {
    public:
        using value_type_simple::value_type_simple;

        virtual value_ptr try_convert_to(const value* val, std::type_index type) const override;
    };
}
