#include "pch.h"
#include "compression.h"
#include "data.h"
#include "file.h"
#include "saved_data.h"
#include "stream.h"

ff::data::saved_data_base::~saved_data_base()
{
}

std::shared_ptr<ff::data::reader_base> ff::data::saved_data_base::loaded_reader() const
{
    return std::make_shared<ff::data::data_reader>(this->loaded_data());
}

std::shared_ptr<ff::data::data_base> ff::data::saved_data_base::loaded_data() const
{
    if (this->type() == saved_data_type::zlib_compressed)
    {
        auto write_buffer = std::make_shared<std::vector<uint8_t>>();
        write_buffer->reserve(this->loaded_size());
        ff::data::data_writer writer(write_buffer);

        if (ff::data::compression::uncompress(*this->saved_reader(), this->saved_size(), writer))
        {
            return std::make_shared<ff::data::data_vector>(write_buffer);
        }
        else
        {
            assert(false);
            return nullptr;
        }
    }

    return this->saved_data();
}

ff::data::saved_data_static::saved_data_static(const std::shared_ptr<data_base>& data, size_t loaded_size, saved_data_type type)
    : data(data)
    , data_loaded_size(loaded_size)
    , data_type(type)
{
}

ff::data::saved_data_static::saved_data_static(const saved_data_static& other)
    : data(other.data)
    , data_loaded_size(other.data_loaded_size)
    , data_type(other.data_type)
{
}

ff::data::saved_data_static::saved_data_static(saved_data_static&& other) noexcept
    : data(std::move(other.data))
    , data_loaded_size(other.data_loaded_size)
    , data_type(other.data_type)
{
    other.data_loaded_size = 0;
    other.data_type = saved_data_type::none;
}

ff::data::saved_data_static& ff::data::saved_data_static::operator=(const saved_data_static& other)
{
    if (this != &other)
    {
        this->data = other.data;
        this->data_loaded_size = other.data_loaded_size;
        this->data_type = other.data_type;
    }

    return *this;
}

ff::data::saved_data_static& ff::data::saved_data_static::operator=(saved_data_static&& other) noexcept
{
    if (this != &other)
    {
        this->data = std::move(other.data);
        this->data_loaded_size = other.data_loaded_size;
        this->data_type = other.data_type;

        other.data_loaded_size = 0;
        other.data_type = saved_data_type::none;
    }

    return *this;
}

void ff::data::saved_data_static::swap(saved_data_static& other)
{
    if (this != &other)
    {
        std::swap(this->data, other.data);
        std::swap(this->data_loaded_size, other.data_loaded_size);
        std::swap(this->data_type, other.data_type);
    }
}

std::shared_ptr<ff::data::reader_base> ff::data::saved_data_static::saved_reader() const
{
    return std::make_shared<ff::data::data_reader>(this->data);
}

std::shared_ptr<ff::data::data_base> ff::data::saved_data_static::saved_data() const
{
    return this->data;
}

size_t ff::data::saved_data_static::saved_size() const
{
    return this->data->size();
}

size_t ff::data::saved_data_static::loaded_size() const
{
    return this->data_loaded_size;
}

ff::data::saved_data_type ff::data::saved_data_static::type() const
{
    return this->data_type;
}

ff::data::saved_data_file::saved_data_file(const std::filesystem::path& path, size_t offset, size_t saved_size, size_t loaded_size, saved_data_type type)
    : path(path)
    , data_offset(offset)
    , data_saved_size(saved_size)
    , data_loaded_size(loaded_size)
    , data_type(type)
{
}

ff::data::saved_data_file::saved_data_file(const saved_data_file& other)
    : path(other.path)
    , data_offset(other.data_offset)
    , data_saved_size(other.data_saved_size)
    , data_loaded_size(other.data_loaded_size)
    , data_type(other.data_type)
{
}

ff::data::saved_data_file::saved_data_file(saved_data_file&& other) noexcept
    : path(std::move(other.path))
    , data_offset(other.data_offset)
    , data_saved_size(other.data_saved_size)
    , data_loaded_size(other.data_loaded_size)
    , data_type(other.data_type)
{
    other.data_offset = 0;
    other.data_saved_size = 0;
    other.data_loaded_size = 0;
    other.data_type = saved_data_type::none;
}

ff::data::saved_data_file& ff::data::saved_data_file::operator=(const saved_data_file& other)
{
    if (this != &other)
    {
        this->path = other.path;
        this->data_offset = other.data_offset;
        this->data_saved_size = other.data_saved_size;
        this->data_loaded_size = other.data_loaded_size;
        this->data_type = other.data_type;
    }

    return *this;
}

ff::data::saved_data_file& ff::data::saved_data_file::operator=(saved_data_file&& other) noexcept
{
    if (this != &other)
    {
        this->path = std::move(other.path);
        this->data_offset = other.data_offset;
        this->data_saved_size = other.data_saved_size;
        this->data_loaded_size = other.data_loaded_size;
        this->data_type = other.data_type;

        other.data_offset = 0;
        other.data_saved_size = 0;
        other.data_loaded_size = 0;
        other.data_type = saved_data_type::none;
    }

    return *this;
}

void ff::data::saved_data_file::swap(saved_data_file& other)
{
    if (this != &other)
    {
        std::swap(this->path, other.path);
        std::swap(this->data_offset, other.data_offset);
        std::swap(this->data_saved_size, other.data_saved_size);
        std::swap(this->data_loaded_size, other.data_loaded_size);
        std::swap(this->data_type, other.data_type);
    }
}

std::shared_ptr<ff::data::reader_base> ff::data::saved_data_file::saved_reader() const
{
    ff::data::file_read file(this->path);
    size_t actual_pos = file.pos(this->data_offset);
    assert(actual_pos == this->data_offset && actual_pos + this->data_saved_size <= file.size());

    return std::make_shared<ff::data::file_reader>(std::move(file));
}

std::shared_ptr<ff::data::data_base> ff::data::saved_data_file::saved_data() const
{
    std::vector<uint8_t> buffer(this->data_saved_size);
    std::shared_ptr<ff::data::reader_base> reader = this->saved_reader();
    size_t actually_read = reader->read(buffer.data(), buffer.size());

    if (actually_read == buffer.size())
    {
        return std::make_shared<ff::data::data_vector>(std::move(buffer));
    }
    else
    {
        assert(false);
        return nullptr;
    }
}

size_t ff::data::saved_data_file::saved_size() const
{
    return this->data_saved_size;
}

size_t ff::data::saved_data_file::loaded_size() const
{
    return this->data_loaded_size;
}

ff::data::saved_data_type ff::data::saved_data_file::type() const
{
    return this->data_type;
}

void std::swap(ff::data::saved_data_static& value1, ff::data::saved_data_static& value2)
{
    value1.swap(value2);
}

void std::swap(ff::data::saved_data_file& value1, ff::data::saved_data_file& value2)
{
    value1.swap(value2);
}
