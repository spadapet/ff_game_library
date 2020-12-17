#include "pch.h"
#include "resource_object_base.h"
#include "resource_object_v.h"

static struct register_values_struct
{
    register_values_struct()
    {
        ff::value::register_type<ff::type::resource_object_type>("resource_object", 100);
    }
} register_values;

ff::type::resource_object_v::resource_object_v(std::shared_ptr<ff::resource_object_base>&& value)
    : value(std::move(value))
{}

ff::type::resource_object_v::resource_object_v(const std::shared_ptr<ff::resource_object_base>& value)
    : value(value)
{}

const std::shared_ptr<ff::resource_object_base>& ff::type::resource_object_v::get() const
{
    return this->value;
}

ff::value* ff::type::resource_object_v::get_static_value(std::shared_ptr<ff::resource_object_base>&& value)
{
    return !value ? resource_object_v::get_static_default_value() : nullptr;
}

ff::value* ff::type::resource_object_v::get_static_value(const std::shared_ptr<ff::resource_object_base>& value)
{
    return !value ? resource_object_v::get_static_default_value() : nullptr;
}

ff::value* ff::type::resource_object_v::get_static_default_value()
{
    static resource_object_v default_value = std::shared_ptr<ff::resource_object_base>();
    return &default_value;
}

ff::value_ptr ff::type::resource_object_type::try_convert_to(const value* val, std::type_index type) const
{
    if (type == typeid(ff::type::dict_v) || type == typeid(ff::type::data_v) || type == typeid(ff::type::saved_data_v))
    {
        auto& src = val->get<ff::resource_object_base>();
        if (src)
        {
            ff::dict dict;
            if (ff::resource_object_base::save_to_cache_typed(*src.get(), dict))
            {
                ff::value_ptr dict_value = ff::value::create<ff::dict>(std::move(dict));

                if (type == typeid(ff::type::dict_v))
                {
                    return dict_value;
                }
                
                if (type == typeid(ff::type::data_v))
                {
                    return dict_value->try_convert<ff::data_base>();
                }

                if (type == typeid(ff::type::saved_data_v))
                {
                    return dict_value->try_convert<ff::saved_data_base>();
                }
            }
        }
    }

    return nullptr;
}

ff::value_ptr ff::type::resource_object_type::load(reader_base& reader) const
{
    return ff::value::load_typed(reader);
}

bool ff::type::resource_object_type::save(const value* val, writer_base& writer) const
{
    return val->try_convert<ff::saved_data_base>()->save_typed(writer);
}

void ff::type::resource_object_type::print(const value* val, std::ostream& output) const
{
    auto& data = val->get<ff::resource_object_base>();
    output << "<resource_object: " << (data ? typeid(*data.get()).name() : "null") << ">";
}
