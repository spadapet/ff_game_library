#include "pch.h"
#include "data.h"
#include "data_v.h"
#include "dict.h"
#include "dict_v.h"
#include "saved_data.h"
#include "saved_data_v.h"
#include "stream.h"

ff::type::dict_v::dict_v(ff::dict&& value)
    : value(std::move(value))
{}

const ff::dict& ff::type::dict_v::get() const
{
    return this->value;
}

ff::value* ff::type::dict_v::get_static_value(ff::dict&& value)
{
    return value.empty() ? dict_v::get_static_default_value() : nullptr;
}

ff::value* ff::type::dict_v::get_static_default_value()
{
    static dict_v default_value = ff::dict();
    return &default_value;
}

ff::value_ptr ff::type::dict_type::try_convert_to(const value* val, std::type_index type) const
{
    if (type == typeid(ff::type::data_v) || type == typeid(ff::type::saved_data_v))
    {
        const ff::dict& dict = val->get<ff::dict>();
        auto buffer = std::make_shared<std::vector<uint8_t>>();

        ff::data_writer writer(buffer);
        if (dict.save(writer))
        {
            auto data = std::make_shared<ff::data_vector>(buffer);
            if (type == typeid(ff::type::data_v))
            {
                return ff::value::create<ff::data_base>(std::move(data));
            }
            else
            {
                auto saved_data = std::make_shared<ff::saved_data_static>(data, data->size(), ff::saved_data_type::dict);
                return ff::value::create<ff::saved_data_base>(std::move(saved_data));
            }
        }
    }

    return nullptr;
}

bool ff::type::dict_type::can_have_named_children() const
{
    return true;
}

ff::value_ptr ff::type::dict_type::named_child(const value* val, std::string_view name) const
{
    const ff::dict& dict = val->get<ff::dict>();
    return dict.get(name);
}

std::vector<std::string_view> ff::type::dict_type::child_names(const value* val) const
{
    const ff::dict& dict = val->get<ff::dict>();
    return dict.child_names();
}

ff::value_ptr ff::type::dict_type::load(reader_base& reader) const
{
    return ff::value::load_typed(reader);
}

bool ff::type::dict_type::save(const value* val, writer_base& writer) const
{
    return val->try_convert<ff::saved_data_base>()->save_typed(writer);
}