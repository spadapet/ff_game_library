#include "pch.h"
#include "data.h"
#include "file.h"
#include "stream.h"

ff::data_base::~data_base()
{}

ff::data_static::data_static(const void* data, size_t size)
    : static_data(reinterpret_cast<const uint8_t*>(data))
    , data_size(size)
{}

size_t ff::data_static::size() const
{
    return this->data_size;
}

const uint8_t* ff::data_static::data() const
{
    return this->static_data;
}

std::shared_ptr<ff::data_base> ff::data_static::subdata(size_t offset, size_t size) const
{
    assert(offset + size <= this->data_size);
    return std::make_shared<ff::data_static>(this->static_data + offset, size);
}

ff::data_mem_mapped::data_mem_mapped(const std::shared_ptr<file_mem_mapped>& file)
    : data_mem_mapped(file, 0, file->size())
{}

ff::data_mem_mapped::data_mem_mapped(const std::shared_ptr<file_mem_mapped>& file, size_t offset, size_t size)
    : shared_file(file)
    , data_offset(offset)
    , data_size(size)
{
    assert(*file && offset + size <= file->size());
}

ff::data_mem_mapped::data_mem_mapped(file_mem_mapped&& file) noexcept
    : data_mem_mapped(std::make_shared<file_mem_mapped>(std::move(file)))
{}

ff::data_mem_mapped::data_mem_mapped(file_mem_mapped&& file, size_t offset, size_t size) noexcept
    : data_mem_mapped(std::make_shared<file_mem_mapped>(std::move(file)), offset, size)
{}

bool ff::data_mem_mapped::valid() const
{
    return this->shared_file && *this->shared_file && (this->data_offset + this->data_size) <= this->shared_file->size();
}

const std::shared_ptr<ff::file_mem_mapped>& ff::data_mem_mapped::file() const
{
    return this->shared_file;
}

size_t ff::data_mem_mapped::offset() const
{
    return this->data_offset;
}

size_t ff::data_mem_mapped::size() const
{
    return this->data_size;
}

const uint8_t* ff::data_mem_mapped::data() const
{
    return this->shared_file->data() + this->data_offset;
}

std::shared_ptr<ff::data_base> ff::data_mem_mapped::subdata(size_t offset, size_t size) const
{
    assert(this->data_offset + offset + size <= this->shared_file->size());
    return std::make_shared<data_mem_mapped>(this->shared_file, this->data_offset + offset, size);
}

ff::data_vector::data_vector(const std::shared_ptr<const std::vector<uint8_t>>& vector)
    : data_vector(vector, 0, vector->size())
{}

ff::data_vector::data_vector(const std::shared_ptr<const std::vector<uint8_t>>& vector, size_t offset, size_t size)
    : shared_vector(vector)
    , data_offset(offset)
    , data_size(size)
{}

ff::data_vector::data_vector(std::vector<uint8_t>&& vector) noexcept
    : data_vector(std::make_shared<const std::vector<uint8_t>>(std::move(vector)))
{}

ff::data_vector::data_vector(std::vector<uint8_t>&& vector, size_t offset, size_t size) noexcept
    : data_vector(std::make_shared<const std::vector<uint8_t>>(std::move(vector)), offset, size)
{}

const std::shared_ptr<const std::vector<uint8_t>>& ff::data_vector::vector() const
{
    return this->shared_vector;
}

size_t ff::data_vector::offset() const
{
    return this->data_offset;
}

size_t ff::data_vector::size() const
{
    return this->data_size;
}

const uint8_t* ff::data_vector::data() const
{
    return this->shared_vector->data() + this->data_offset;
}

std::shared_ptr<ff::data_base> ff::data_vector::subdata(size_t offset, size_t size) const
{
    assert(this->data_offset + offset + size <= this->shared_vector->size());
    return std::make_shared<data_vector>(this->shared_vector, this->data_offset + offset, size);
}
