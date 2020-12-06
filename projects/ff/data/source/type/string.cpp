#include "pch.h"
#include "string.h"
#include "../persist.h"
#include "../value/value.h"

ff::data::type::string::string(std::string&& value)
    : value(std::move(value))
{}

ff::data::type::string::string(const std::string & value)
    : value(value)
{}

const std::string& ff::data::type::string::get() const
{
    return this->value;
}

ff::data::value* ff::data::type::string::get_static_value(std::string&& value)
{
    return value.empty() ? string::get_static_default_value() : nullptr;
}

ff::data::value* ff::data::type::string::get_static_value(const std::string& value)
{
    return value.empty() ? string::get_static_default_value() : nullptr;
}

ff::data::value* ff::data::type::string::get_static_default_value()
{
    static string default_value = std::string();
    return &default_value;
}
