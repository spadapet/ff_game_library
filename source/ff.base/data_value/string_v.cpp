#include "pch.h"
#include "types/fixed.h"
#include "types/point.h"
#include "types/rect.h"
#include "data_value/bool_v.h"
#include "data_value/double_v.h"
#include "data_value/fixed_v.h"
#include "data_value/float_v.h"
#include "data_value/int_v.h"
#include "data_value/null_v.h"
#include "data_value/point_v.h"
#include "data_value/rect_v.h"
#include "data_value/size_v.h"
#include "data_value/string_v.h"
#include "data_value/uuid_v.h"

ff::type::string_v::string_v(std::string&& value)
    : value(std::move(value))
{}

ff::type::string_v::string_v(const std::string& value)
    : value(value)
{}

ff::type::string_v::~string_v()
{}

const std::string& ff::type::string_v::get() const
{
    return this->value;
}

ff::value* ff::type::string_v::get_static_value(const std::string& value)
{
    if (value.empty())
    {
        return string_v::get_static_default_value();
    }
    else if (value == "true")
    {
        static string_v true_value("true");
        return &true_value;
    }
    else if (value == "false")
    {
        static string_v false_value("false");
        return &false_value;
    }
    else if (value == "null")
    {
        static string_v null_value("null");
        return &null_value;
    }
    
    return nullptr;
}

ff::value* ff::type::string_v::get_static_default_value()
{
    static string_v default_value = std::string();
    return &default_value;
}

ff::value_ptr ff::type::string_type::try_convert_to(const value* val, std::type_index type) const
{
    const std::string& src = val->get<std::string>();
    const char* start = src.c_str();
    char* end = nullptr;
    size_t chars_read = 0;

    if (type == typeid(ff::type::bool_v))
    {
        if (src.empty() || src == "false" || src == "no" || src == "0")
        {
            return ff::value::create<bool>(false);
        }
        else if (src == "true" || src == "yes" || src == "1")
        {
            return ff::value::create<bool>(false);
        }
    }

    if (type == typeid(ff::type::double_v))
    {
        double val = std::strtod(start, &end);
        if (end > start && !*end)
        {
            return ff::value::create<double>(val);
        }
    }

    if (type == typeid(ff::type::fixed_v))
    {
        double val = std::strtod(start, &end);
        if (end > start && !*end)
        {
            return ff::value::create<ff::fixed_int>(val);
        }
    }

    if (type == typeid(ff::type::float_v))
    {
        double val = std::strtod(start, &end);
        if (end > start && !*end)
        {
            return ff::value::create<float>(static_cast<float>(val));
        }
    }

    if (type == typeid(ff::type::int_v))
    {
        long val = std::strtol(start, &end, 10);
        if (end > start && !*end)
        {
            return ff::value::create<int>(static_cast<int>(val));
        }
    }

    if (type == typeid(ff::type::null_v) && src == "null")
    {
        return ff::value::create<nullptr_t>();
    }

    if (type == typeid(ff::type::size_v))
    {
        unsigned long val = std::strtoul(start, &end, 10);
        if (end > start && !*end)
        {
            return ff::value::create<size_t>(static_cast<size_t>(val));
        }
    }

    if (type == typeid(ff::type::point_int_v))
    {
        ff::point_int point{};
        if (::_snscanf_s(start, src.size(), "[ %d, %d ]%n", &point.x, &point.y, &chars_read) == 2 && chars_read == src.size())
        {
            return ff::value::create<ff::point_int>(point);
        }
    }

    if (type == typeid(ff::type::point_float_v))
    {
        ff::point_float point{};
        if (::_snscanf_s(start, src.size(), "[ %g, %g ]%n", &point.x, &point.y, &chars_read) == 2 && chars_read == src.size())
        {
            return ff::value::create<ff::point_float>(point);
        }
    }

    if (type == typeid(ff::type::point_double_v))
    {
        ff::point_double point{};
        if (::_snscanf_s(start, src.size(), "[ %lg, %lg ]%n", &point.x, &point.y, &chars_read) == 2 && chars_read == src.size())
        {
            return ff::value::create<ff::point_double>(point);
        }
    }

    if (type == typeid(ff::type::point_fixed_v))
    {
        ff::point_double point{};
        if (::_snscanf_s(start, src.size(), "[ %lg, %lg ]%n", &point.x, &point.y, &chars_read) == 2 && chars_read == src.size())
        {
            return ff::value::create<ff::point_fixed>(point.cast<ff::fixed_int>());
        }
    }

    if (type == typeid(ff::type::rect_int_v))
    {
        ff::rect_int rect{};
        if (::_snscanf_s(start, src.size(), "[ %d, %d, %d, %d ]%n", &rect.left, &rect.top, &rect.right, &rect.bottom, &chars_read) == 4 && chars_read == src.size())
        {
            return ff::value::create<ff::rect_int>(rect);
        }
    }

    if (type == typeid(ff::type::rect_float_v))
    {
        ff::rect_float rect{};
        if (::_snscanf_s(start, src.size(), "[ %g, %g, %g, %g ]%n", &rect.left, &rect.top, &rect.right, &rect.bottom, &chars_read) == 4 && chars_read == src.size())
        {
            return ff::value::create<ff::rect_float>(rect);
        }
    }

    if (type == typeid(ff::type::rect_double_v))
    {
        ff::rect_double rect{};
        if (::_snscanf_s(start, src.size(), "[ %lg, %lg, %lg, %lg ]%n", &rect.left, &rect.top, &rect.right, &rect.bottom, &chars_read) == 4 && chars_read == src.size())
        {
            return ff::value::create<ff::rect_double>(rect);
        }
    }

    if (type == typeid(ff::type::rect_fixed_v))
    {
        ff::rect_double rect{};
        if (::_snscanf_s(start, src.size(), "[ %lg, %lg, %lg, %lg ]%n", &rect.left, &rect.top, &rect.right, &rect.bottom, &chars_read) == 4 && chars_read == src.size())
        {
            return ff::value::create<ff::rect_fixed>(rect.cast<ff::fixed_int>());
        }
    }

    if (type == typeid(ff::type::uuid_v))
    {
        ff::uuid uuid;
        if (ff::uuid::from_string(src, uuid))
        {
            return ff::value::create<ff::uuid>(uuid);
        }
    }

    return nullptr;
}
