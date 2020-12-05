#pragma once

#include "../value/value.h"

namespace ff::data::type
{
    class int32 : public ff::data::value
    {
    public:
        int32(int value);

        std::int32_t get() const;
        static ff::data::value* get_static_value(int value);
        static ff::data::value* get_static_default_value();

    private:
        int32();

        int value;
    };

    class int32_type : public ff::data::value_type_base<ff::data::type::int32>
    {
    public:
        //virtual value_ptr try_convert_to(const value* val, std::type_index type) const;
        //virtual value_ptr load(reader_base& reader) const;
        //virtual bool save(const value* val, writer_base& writer) const;
    };
}

namespace ff::data
{
    template<>
    struct value_traits<int>
    {
        using value_derived_type = typename ff::data::type::int32;
        using get_type = typename std::int32_t;
    };
};
