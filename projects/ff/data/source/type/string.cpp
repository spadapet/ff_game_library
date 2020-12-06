#include "pch.h"
#include "string.h"
#include "../persist.h"
#include "../value/value.h"

ff::type::string::string(std::string&& value)
    : value(std::move(value))
{}

ff::type::string::string(const std::string & value)
    : value(value)
{}

const std::string& ff::type::string::get() const
{
    return this->value;
}

ff::value* ff::type::string::get_static_value(std::string&& value)
{
    return value.empty() ? string::get_static_default_value() : nullptr;
}

ff::value* ff::type::string::get_static_value(const std::string& value)
{
    return value.empty() ? string::get_static_default_value() : nullptr;
}

ff::value* ff::type::string::get_static_default_value()
{
    static string default_value = std::string();
    return &default_value;
}
