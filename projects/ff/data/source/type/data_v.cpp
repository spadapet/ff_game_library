#include "pch.h"
#include "data_v.h"
#include "saved_data_v.h"
#include "../data.h"
#include "../saved_data.h"

ff::type::data_v::data_v(std::shared_ptr<ff::data_base>&& value)
    : value(std::move(value))
{}

ff::type::data_v::data_v(const std::shared_ptr<ff::data_base>& value)
    : value(value)
{}

const std::shared_ptr<ff::data_base>& ff::type::data_v::get() const
{
    return this->value;
}

ff::value* ff::type::data_v::get_static_value(std::shared_ptr<ff::data_base>&& value)
{
    return !value ? data_v::get_static_default_value() : nullptr;
}

ff::value* ff::type::data_v::get_static_value(const std::shared_ptr<ff::data_base>& value)
{
    return !value ? data_v::get_static_default_value() : nullptr;
}

ff::value* ff::type::data_v::get_static_default_value()
{
    static data_v default_value = std::shared_ptr<ff::data_base>();
    return &default_value;
}

ff::value_ptr ff::type::data_type::try_convert_to(const value* val, std::type_index type) const
{
    if (type == typeid(ff::type::saved_data_v))
    {
        auto& data = val->get<ff::data_base>();
        auto saved_data = data ? std::make_shared<ff::saved_data_static>(data, data->size(), ff::saved_data_type::none) : nullptr;
        return ff::value::create<ff::saved_data_base>(std::move(saved_data));
    }

    return nullptr;
}

ff::value_ptr ff::type::data_type::load(reader_base& reader) const
{
    return ff::value::load_typed(reader);
}

bool ff::type::data_type::save(const value* val, writer_base& writer) const
{
    return val->try_convert<ff::saved_data_base>()->save_typed(writer);
}

void ff::type::data_type::print(const value* val, std::ostream& output) const
{
    auto& data = val->get<ff::data_base>();
    output << "<data: " << (data ? data->size() : 0) << ">";
}
