#include "pch.h"
#include "data.h"
#include "file.h"
#include "stream.h"

ff::data::data_base::~data_base()
{
}

ff::data::data_static::data_static(const void* data, size_t size)
    : static_data(reinterpret_cast<const uint8_t*>(data))
    , data_size(size)
{
}

ff::data::data_static::data_static(const data_static& other)
    : static_data(other.static_data)
    , data_size(other.data_size)
{
}

ff::data::data_static& ff::data::data_static::operator=(const data_static& other)
{
    this->static_data = other.static_data;
    this->data_size = other.data_size;
    return *this;
}

void ff::data::data_static::swap(data_static& other)
{
    std::swap(this->static_data, other.static_data);
    std::swap(this->data_size, other.data_size);
}

size_t ff::data::data_static::size() const
{
    return this->data_size;
}

const uint8_t* ff::data::data_static::data() const
{
    return this->static_data;
}

std::shared_ptr<ff::data::data_base> ff::data::data_static::subdata(size_t offset, size_t size) const
{
    assert(offset + size <= this->data_size);
    return std::make_shared<ff::data::data_static>(this->static_data + offset, size);
}

ff::data::data_mem_mapped::data_mem_mapped(const std::shared_ptr<file_mem_mapped>& file)
    : data_mem_mapped(file, 0, file->size())
{
}

ff::data::data_mem_mapped::data_mem_mapped(const std::shared_ptr<file_mem_mapped>& file, size_t offset, size_t size)
    : shared_file(file)
    , data_offset(offset)
    , data_size(size)
{
    assert(*file && offset + size <= file->size());
}

ff::data::data_mem_mapped::data_mem_mapped(const data_mem_mapped& other)
    : data_mem_mapped(other.shared_file, other.data_offset, other.data_size)
{
}

ff::data::data_mem_mapped::data_mem_mapped(data_mem_mapped&& other) noexcept
    : shared_file(std::move(other.shared_file))
    , data_offset(other.data_offset)
    , data_size(other.data_size)
{
    other.data_offset = 0;
    other.data_size = 0;
}

ff::data::data_mem_mapped::data_mem_mapped(file_mem_mapped&& file) noexcept
    : data_mem_mapped(std::make_shared<file_mem_mapped>(std::move(file)))
{
}

ff::data::data_mem_mapped& ff::data::data_mem_mapped::operator=(const data_mem_mapped& other)
{
    if (this != &other)
    {
        this->shared_file = other.shared_file;
        this->data_offset = other.data_offset;
        this->data_size = other.data_size;
    }

    return *this;
}

ff::data::data_mem_mapped& ff::data::data_mem_mapped::operator=(data_mem_mapped&& other) noexcept
{
    if (this != &other)
    {
        this->shared_file = std::move(other.shared_file);
        this->data_offset = other.data_offset;
        this->data_size = other.data_size;

        other.data_offset = 0;
        other.data_size = 0;
    }

    return *this;
}

void ff::data::data_mem_mapped::swap(data_mem_mapped& other)
{
    if (this != &other)
    {
        std::swap(this->shared_file, other.shared_file);
        std::swap(this->data_offset, other.data_offset);
        std::swap(this->data_size, other.data_size);
    }
}

const std::shared_ptr<ff::data::file_mem_mapped>& ff::data::data_mem_mapped::file() const
{
    return this->shared_file;
}

size_t ff::data::data_mem_mapped::offset() const
{
    return this->data_offset;
}

size_t ff::data::data_mem_mapped::size() const
{
    return this->data_size;
}

const uint8_t* ff::data::data_mem_mapped::data() const
{
    return this->shared_file->data() + this->data_offset;
}

std::shared_ptr<ff::data::data_base> ff::data::data_mem_mapped::subdata(size_t offset, size_t size) const
{
    assert(this->data_offset + offset + size <= this->shared_file->size());
    return std::make_shared<data_mem_mapped>(this->shared_file, this->data_offset + offset, size);
}

ff::data::data_vector::data_vector(const std::shared_ptr<const std::vector<uint8_t>>& vector)
    : data_vector(vector, 0, vector->size())
{
}

ff::data::data_vector::data_vector(const std::shared_ptr<const std::vector<uint8_t>>& vector, size_t offset, size_t size)
    : shared_vector(vector)
    , data_offset(offset)
    , data_size(size)
{
}

ff::data::data_vector::data_vector(std::vector<uint8_t>&& vector)
    : data_vector(std::make_shared<const std::vector<uint8_t>>(std::move(vector)))
{
}

ff::data::data_vector::data_vector(std::vector<uint8_t>&& vector, size_t offset, size_t size)
    : data_vector(std::make_shared<const std::vector<uint8_t>>(std::move(vector)), offset, size)
{
}

ff::data::data_vector::data_vector(const data_vector& other)
    : shared_vector(other.shared_vector)
    , data_offset(other.data_offset)
    , data_size(other.data_size)
{
}

ff::data::data_vector::data_vector(data_vector&& other) noexcept
    : shared_vector(std::move(other.shared_vector))
    , data_offset(other.data_offset)
    , data_size(other.data_size)
{
    other.data_offset = 0;
    other.data_size = 0;
}

ff::data::data_vector& ff::data::data_vector::operator=(const data_vector& other)
{
    if (this != &other)
    {
        this->shared_vector = other.shared_vector;
        this->data_offset = other.data_offset;
        this->data_size = other.data_size;
    }

    return *this;
}

ff::data::data_vector& ff::data::data_vector::operator=(data_vector&& other) noexcept
{
    if (this != &other)
    {
        this->shared_vector = std::move(other.shared_vector);
        this->data_offset = other.data_offset;
        this->data_size = other.data_size;

        other.data_offset = 0;
        other.data_size = 0;
    }

    return *this;
}

void ff::data::data_vector::swap(data_vector& other)
{
    if (this != &other)
    {
        std::swap(this->shared_vector, other.shared_vector);
        std::swap(this->data_offset, other.data_offset);
        std::swap(this->data_size, other.data_size);
    }
}

const std::shared_ptr<const std::vector<uint8_t>>& ff::data::data_vector::vector() const
{
    return this->shared_vector;
}

size_t ff::data::data_vector::offset() const
{
    return this->data_offset;
}

size_t ff::data::data_vector::size() const
{
    return this->data_size;
}

const uint8_t* ff::data::data_vector::data() const
{
    return this->shared_vector->data() + this->data_offset;
}

std::shared_ptr<ff::data::data_base> ff::data::data_vector::subdata(size_t offset, size_t size) const
{
    assert(this->data_offset + offset + size <= this->shared_vector->size());
    return std::make_shared<data_vector>(this->shared_vector, this->data_offset + offset, size);
}

void std::swap(ff::data::data_static& value1, ff::data::data_static& value2)
{
    value1.swap(value2);
}

void std::swap(ff::data::data_mem_mapped& value1, ff::data::data_mem_mapped& value2)
{
    value1.swap(value2);
}

void std::swap(ff::data::data_vector& value1, ff::data::data_vector& value2)
{
    value1.swap(value2);
}
