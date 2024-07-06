#include "pch.h"
#include "data.h"
#include "data_v.h"
#include "dict_v.h"
#include "saved_data.h"
#include "saved_data_v.h"
#include "stream.h"

ff::type::saved_data_v::saved_data_v(std::shared_ptr<ff::saved_data_base>&& value)
    : value(std::move(value))
{}

ff::type::saved_data_v::saved_data_v(const std::shared_ptr<ff::saved_data_base>& value)
    : value(value)
{}

const std::shared_ptr<ff::saved_data_base>& ff::type::saved_data_v::get() const
{
    return this->value;
}

ff::value* ff::type::saved_data_v::get_static_value(const std::shared_ptr<ff::saved_data_base>& value)
{
    return !value ? saved_data_v::get_static_default_value() : nullptr;
}

ff::value* ff::type::saved_data_v::get_static_default_value()
{
    static saved_data_v default_value = std::shared_ptr<ff::saved_data_base>();
    return &default_value;
}

ff::value_ptr ff::type::saved_data_type::try_convert_to(const value* val, std::type_index type) const
{
    if (type == typeid(ff::type::data_v) || type == typeid(ff::type::dict_v))
    {
        auto& saved_data = val->get<ff::saved_data_base>();
        if (saved_data && (type != typeid(ff::type::dict_v) || ff::flags::has(saved_data->type(), ff::saved_data_type::dict)))
        {
            auto data = saved_data->loaded_data();
            return ff::value::create<ff::data_base>(data, saved_data->type())->try_convert(type);
        }
    }

    return nullptr;
}

ff::value_ptr ff::type::saved_data_type::load(reader_base& reader) const
{
    size_t saved_size;
    size_t loaded_size;
    ff::saved_data_type type;

    if (ff::load(reader, saved_size) && ff::load(reader, loaded_size) && ff::load(reader, type))
    {
        size_t start_pos = reader.pos();
        size_t new_pos = reader.pos(start_pos + saved_size);
        if (new_pos == start_pos + saved_size && ff::load_padding(reader, saved_size))
        {
            auto saved_data = reader.saved_data(start_pos, saved_size, loaded_size, type);
            if (saved_data)
            {
                return ff::value::create<ff::saved_data_base>(saved_data);
            }
        }
    }

    return nullptr;
}

bool ff::type::saved_data_type::save(const value* val, writer_base& writer) const
{
    std::shared_ptr<ff::saved_data_base> saved_data = val->get<ff::saved_data_base>();
    size_t saved_size = saved_data ? saved_data->saved_size() : 0;
    size_t loaded_size = saved_data ? saved_data->loaded_size() : 0;
    ff::saved_data_type type = saved_data ? saved_data->type() : ff::saved_data_type::none;

    if (ff::save(writer, saved_size) && ff::save(writer, loaded_size) && ff::save(writer, type))
    {
        if (!saved_data)
        {
            return true;
        }

        auto saved_reader = saved_data->saved_reader();
        if (!saved_reader)
        {
            assert(false);
            return false;
        }

        size_t written = ff::stream_copy(writer, *saved_reader, saved_size);
        return (written == saved_size) && ff::save_padding(writer, saved_size);
    }

    return false;
}

void ff::type::saved_data_type::print(const value* val, std::ostream& output) const
{
    std::shared_ptr<ff::saved_data_base> saved_data = val->get<ff::saved_data_base>();
    size_t saved_size = saved_data ? saved_data->saved_size() : 0;
    size_t loaded_size = saved_data ? saved_data->loaded_size() : 0;

    if (loaded_size && saved_size != loaded_size)
    {
        double saved_size_d = static_cast<double>(saved_size);
        double loaded_size_d = static_cast<double>(loaded_size);

        output
            << "<saved_data: "
            << saved_size
            << " -> "
            << loaded_size
            << " ("
            << std::setprecision(3)
            << saved_size_d * 100.0 / loaded_size_d << "%)>";
    }
    else
    {
        output << "<saved_data: " << loaded_size << ">";
    }
}
