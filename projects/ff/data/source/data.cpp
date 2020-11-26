#include "pch.h"
#include "data.h"

ff::data::data_static::data_static(const void* data, size_t size)
{}

ff::data::data_static::data_static(const data_static & other)
{}

ff::data::data_static& ff::data::data_static::operator=(const data_static & other)
{
    return *this;
}

void ff::data::data_static::swap(const data_static& other)
{}

size_t ff::data::data_static::size() const
{
    return size_t();
}

const uint8_t* ff::data::data_static::data() const
{
    return nullptr;
}

std::unique_ptr<ff::data::data_base> ff::data::data_static::subdata(size_t offset, size_t size)
{
    return std::unique_ptr<data_base>();
}

ff::data::data_mem_mapped::data_mem_mapped(const std::shared_ptr<ff::data::file_mem_mapped>& file)
{}

ff::data::data_mem_mapped::data_mem_mapped(const std::shared_ptr<ff::data::file_mem_mapped>&file, size_t offset, size_t size)
{}

ff::data::data_mem_mapped::data_mem_mapped(const data_mem_mapped & other)
{}

ff::data::data_mem_mapped& ff::data::data_mem_mapped::operator=(const data_mem_mapped & other)
{
    return *this;
}

void ff::data::data_mem_mapped::swap(const data_mem_mapped& other)
{}

const std::shared_ptr<ff::data::file_mem_mapped>& ff::data::data_mem_mapped::file() const
{
    return this->shared_file;
}

size_t ff::data::data_mem_mapped::offset() const
{
    return size_t();
}

size_t ff::data::data_mem_mapped::size() const
{
    return size_t();
}

const uint8_t* ff::data::data_mem_mapped::data() const
{
    return nullptr;
}

std::unique_ptr<ff::data::data_base> ff::data::data_mem_mapped::subdata(size_t offset, size_t size)
{
    return std::unique_ptr<data_base>();
}

ff::data::data_vector::data_vector(const std::shared_ptr<const std::vector<uint8_t>>& vector)
{}

ff::data::data_vector::data_vector(const std::shared_ptr<const std::vector<uint8_t>>&vector, size_t offset, size_t size)
{}

ff::data::data_vector::data_vector(const data_vector & other)
{}

ff::data::data_vector& ff::data::data_vector::operator=(const data_vector & other)
{
    return *this;
}

void ff::data::data_vector::swap(const data_vector& other)
{}

const std::shared_ptr<const std::vector<uint8_t>>& ff::data::data_vector::vector() const
{
    return this->shared_vector;
}

size_t ff::data::data_vector::offset() const
{
    return size_t();
}

size_t ff::data::data_vector::size() const
{
    return size_t();
}

const uint8_t* ff::data::data_vector::data() const
{
    return nullptr;
}

std::unique_ptr<ff::data::data_base> ff::data::data_vector::subdata(size_t offset, size_t size)
{
    return std::unique_ptr<data_base>();
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
