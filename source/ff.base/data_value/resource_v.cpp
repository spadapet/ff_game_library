#include "pch.h"
#include "data_value/resource_v.h"
#include "data_value/string_v.h"
#include "resource/resource.h"
#include "resource/resource_load.h"

ff::type::resource_v::resource_v(std::shared_ptr<ff::resource>&& value)
    : value(std::move(value))
{}

ff::type::resource_v::resource_v(const std::shared_ptr<ff::resource>& value)
    : value(value)
{}

const std::shared_ptr<ff::resource>& ff::type::resource_v::get() const
{
    return this->value;
}

ff::value* ff::type::resource_v::get_static_value(const std::shared_ptr<ff::resource>& value)
{
    return !value ? resource_v::get_static_default_value() : nullptr;
}

ff::value* ff::type::resource_v::get_static_default_value()
{
    static resource_v default_value = std::shared_ptr<ff::resource>();
    return &default_value;
}

ff::value_ptr ff::type::resource_type::try_convert_to(const value* val, std::type_index type) const
{
    std::shared_ptr<ff::resource> src = val->get<ff::resource>();
    if (src && type == typeid(ff::type::string_v))
    {
        std::string ref_str(ff::internal::REF_PREFIX);
        ref_str += src->name();
        return ff::value::create<std::string>(std::move(ref_str));
    }

    // Do not access src->value() because it may be loading forever during resource compilation
    // (when the resource reference doesn't exist)
    return nullptr;
}

ff::value_ptr ff::type::resource_type::load(reader_base& reader) const
{
    // Load as a string, the resource loader will convert strings to resource references
    std::string str;
    if (ff::load(reader, str))
    {
        if (str.empty())
        {
            std::shared_ptr<ff::resource> res_val;
            return ff::value::create<ff::resource>(res_val);
        }

        str.insert(0, ff::internal::REF_PREFIX);
        return ff::value::create<std::string>(std::move(str));
    }

    return nullptr;
}

bool ff::type::resource_type::save(const value* val, writer_base& writer) const
{
    auto& src = val->get<ff::resource>();
    std::string name = src ? std::string(src->name()) : std::string();
    return ff::save(writer, name);
}

void ff::type::resource_type::print(const value* val, std::ostream& output) const
{
    auto& data = val->get<ff::resource>();
    output << "<resource: " << (data ? data->name() : "<null>") << ">";
}
