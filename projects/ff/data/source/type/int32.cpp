#include "pch.h"
#include "int32.h"
#include "../value/value.h"

ff::data::type::int32::int32()
{}

ff::data::type::int32::int32(int value)
    : value(value)
{}

std::int32_t ff::data::type::int32::get() const
{
    return this->value;
}

ff::data::value* ff::data::type::int32::get_static_value(int value)
{
    return nullptr;
}

ff::data::value* ff::data::type::int32::get_static_default_value()
{
    return nullptr;
}

//ff::data::value_ptr ff::data::type::int32_type::try_convert_to(const ff::data::value* val, std::type_index type) const
//{
//    return value_ptr();
//}
//
//ff::data::value_ptr ff::data::type::int32_type::load(ff::data::reader_base& reader) const
//{
//    return value_ptr();
//}
//
//bool ff::data::type::int32_type::save(const ff::data::value* val, ff::data::writer_base& writer) const
//{
//    return false;
//}
