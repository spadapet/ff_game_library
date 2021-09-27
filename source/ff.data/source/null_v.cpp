#include "pch.h"
#include "string_v.h"
#include "null_v.h"

std::nullptr_t ff::type::null_v::get() const
{
    return nullptr;
}

ff::value* ff::type::null_v::get_static_value()
{
    return null_v::get_static_default_value();
}

ff::value* ff::type::null_v::get_static_default_value()
{
    static ff::type::null_v default_value;
    return &default_value;
}

ff::value_ptr ff::type::null_type::try_convert_to(const value* val, std::type_index type) const
{
    if (type == typeid(ff::type::string_v))
    {
        return ff::value::create<std::string>("null");
    }

    return nullptr;
}

ff::value_ptr ff::type::null_type::load(reader_base& reader) const
{
    return ff::value::create<std::nullptr_t>();
}

bool ff::type::null_type::save(const value* val, writer_base& writer) const
{
    return true;
}
