#include "pch.h"
#include "compression.h"
#include "data.h"
#include "data_v.h"
#include "dict.h"
#include "dict_v.h"
#include "saved_data.h"
#include "saved_data_v.h"
#include "stream.h"

ff::type::data_v::data_v(std::shared_ptr<ff::data_base>&& value, ff::saved_data_type saved_data_type)
    : value(std::move(value))
    , saved_data_type_(saved_data_type)
{}

ff::type::data_v::data_v(const std::shared_ptr<ff::data_base>& value, ff::saved_data_type saved_data_type)
    : value(value)
    , saved_data_type_(saved_data_type)
{}

const std::shared_ptr<ff::data_base>& ff::type::data_v::get() const
{
    return this->value;
}

ff::saved_data_type ff::type::data_v::saved_data_type() const
{
    return this->saved_data_type_;
}

ff::value* ff::type::data_v::get_static_value(std::shared_ptr<ff::data_base>&& value, ff::saved_data_type)
{
    return !value ? data_v::get_static_default_value() : nullptr;
}

ff::value* ff::type::data_v::get_static_value(const std::shared_ptr<ff::data_base>& value, ff::saved_data_type)
{
    return !value ? data_v::get_static_default_value() : nullptr;
}

ff::value* ff::type::data_v::get_static_default_value()
{
    static data_v default_value(std::shared_ptr<ff::data_base>(), ff::saved_data_type::none);
    return &default_value;
}

ff::value_ptr ff::type::data_type::try_convert_to(const value* val, std::type_index type) const
{
    if (type == typeid(ff::type::saved_data_v))
    {
        auto& data = val->get<ff::data_base>();
        ff::saved_data_type saved_data_type = static_cast<const data_v*>(val)->saved_data_type();

        if (data && data->size() && ff::flags::has(saved_data_type, ff::saved_data_type::zlib_compressed))
        {
            auto buffer_compressed = std::make_shared<std::vector<uint8_t>>();
            buffer_compressed->reserve(data->size());

            if (ff::compression::compress(ff::data_reader(data), data->size(), ff::data_writer(buffer_compressed)))
            {
                auto saved_data = std::make_shared<ff::saved_data_static>(std::make_shared<ff::data_vector>(buffer_compressed), data->size(), saved_data_type);
                return ff::value::create<ff::saved_data_base>(saved_data);
            }
        }
        else
        {
            auto saved_data = data ? std::make_shared<ff::saved_data_static>(data, data->size(), ff::flags::clear(saved_data_type, ff::saved_data_type::zlib_compressed)) : nullptr;
            return ff::value::create<ff::saved_data_base>(saved_data);
        }
    }

    if (type == typeid(ff::type::dict_v))
    {
        auto& data = val->get<ff::data_base>();
        ff::saved_data_type saved_data_type = static_cast<const data_v*>(val)->saved_data_type();

        if (data && ff::flags::has(saved_data_type, ff::saved_data_type::dict))
        {
            ff::dict dict;
            if (ff::dict::load(ff::data_reader(data), dict))
            {
                return ff::value::create<ff::dict>(std::move(dict));
            }
        }
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
