#include "pch.h"
#include "compression.h"
#include "data.h"
#include "file.h"
#include "saved_data.h"
#include "stream.h"

ff::saved_data_base::~saved_data_base()
{}

std::shared_ptr<ff::reader_base> ff::saved_data_base::loaded_reader() const
{
    return std::make_shared<data_reader>(this->loaded_data());
}

std::shared_ptr<ff::data_base> ff::saved_data_base::loaded_data() const
{
    if (ff::flags::has_all(this->type(), saved_data_type::zlib_compressed))
    {
        auto write_buffer = std::make_shared<std::vector<uint8_t>>();
        write_buffer->reserve(this->loaded_size());
        data_writer writer(write_buffer);

        if (ff::compression::uncompress(*this->saved_reader(), this->saved_size(), writer))
        {
            return std::make_shared<data_vector>(write_buffer);
        }
        else
        {
            assert(false);
            return nullptr;
        }
    }

    return this->saved_data();
}

ff::saved_data_static::saved_data_static(const std::shared_ptr<data_base>& data, size_t loaded_size, saved_data_type type)
    : data(data)
    , data_loaded_size(loaded_size)
    , data_type(type)
{}

std::shared_ptr<ff::reader_base> ff::saved_data_static::saved_reader() const
{
    return std::make_shared<data_reader>(this->data);
}

std::shared_ptr<ff::data_base> ff::saved_data_static::saved_data() const
{
    return this->data;
}

size_t ff::saved_data_static::saved_size() const
{
    return this->data->size();
}

size_t ff::saved_data_static::loaded_size() const
{
    return this->data_loaded_size;
}

ff::saved_data_type ff::saved_data_static::type() const
{
    return this->data_type;
}

ff::saved_data_file::saved_data_file(const std::filesystem::path& path, size_t offset, size_t saved_size, size_t loaded_size, saved_data_type type)
    : path(path)
    , data_offset(offset)
    , data_saved_size(saved_size)
    , data_loaded_size(loaded_size)
    , data_type(type)
{}

std::shared_ptr<ff::reader_base> ff::saved_data_file::saved_reader() const
{
    file_read file(this->path);
    if (file)
    {
        size_t actual_pos = file.pos(this->data_offset);
        if (actual_pos == this->data_offset && actual_pos + this->data_saved_size <= file.size())
        {
            return std::make_shared<file_reader>(std::move(file));
        }
    }

    assert(false);
    return nullptr;
}

std::shared_ptr<ff::data_base> ff::saved_data_file::saved_data() const
{
    std::vector<uint8_t> buffer(this->data_saved_size);
    std::shared_ptr<reader_base> reader = this->saved_reader();
    size_t actually_read = reader->read(buffer.data(), buffer.size());

    if (actually_read == this->data_saved_size)
    {
        return std::make_shared<data_vector>(std::move(buffer));
    }

    assert(false);
    return nullptr;
}

size_t ff::saved_data_file::saved_size() const
{
    return this->data_saved_size;
}

size_t ff::saved_data_file::loaded_size() const
{
    return this->data_loaded_size;
}

ff::saved_data_type ff::saved_data_file::type() const
{
    return this->data_type;
}
