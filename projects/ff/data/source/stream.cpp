#include "pch.h"
#include "data.h"
#include "saved_data.h"
#include "stream.h"

ff::data::stream_base::~stream_base()
{}

ff::data::data_reader::data_reader(const std::shared_ptr<data_base>& data)
    : data(data)
    , data_pos(0)
{}

size_t ff::data::data_reader::read(void* data, size_t size)
{
    size = std::min(size, this->data->size() - this->data_pos);
    std::memcpy(data, this->data->data() + this->data_pos, size);
    this->data_pos += size;
    return size;
}

size_t ff::data::data_reader::size() const
{
    return this->data->size();
}

size_t ff::data::data_reader::pos() const
{
    return this->data_pos;
}

size_t ff::data::data_reader::pos(size_t new_pos)
{
    assert(new_pos <= this->size());
    new_pos = std::min(new_pos, this->size());
    this->data_pos = new_pos;
    return new_pos;
}

std::shared_ptr<ff::data::saved_data_base> ff::data::data_reader::saved_data(size_t offset, size_t saved_size, size_t loaded_size, saved_data_type type) const
{
    auto subdata = this->data->subdata(offset, saved_size);
    return std::make_shared<saved_data_static>(subdata, loaded_size, type);
}

ff::data::file_reader::file_reader(file_read&& file)
    : file(std::move(file))
{}

size_t ff::data::file_reader::read(void* data, size_t size)
{
    return file.read(data, size);
}

size_t ff::data::file_reader::size() const
{
    return file.size();
}

size_t ff::data::file_reader::pos() const
{
    return file.pos();
}

size_t ff::data::file_reader::pos(size_t new_pos)
{
    return file.pos(new_pos);
}

std::shared_ptr<ff::data::saved_data_base> ff::data::file_reader::saved_data(size_t offset, size_t saved_size, size_t loaded_size, saved_data_type type) const
{
    assert(offset + saved_size <= this->size());
    return std::make_shared<saved_data_file>(this->file.path(), offset, saved_size, loaded_size, type);
}

ff::data::data_writer::data_writer(const std::shared_ptr<std::vector<uint8_t>>& data)
    : data_writer(data, data->size())
{}

ff::data::data_writer::data_writer(const std::shared_ptr<std::vector<uint8_t>>& data, size_t pos)
    : data(data)
    , data_pos(pos)
{}

size_t ff::data::data_writer::write(const void* data, size_t size)
{
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data);
    size_t copy_size = std::min(size, this->size() - this->data_pos);

    if (copy_size)
    {
        std::memcpy(this->data->data() + this->data_pos, bytes, copy_size);
        this->data_pos += copy_size;
        bytes += copy_size;
        size -= copy_size;
    }

    if (size)
    {
        this->data->insert(this->data->cend(), std::initializer_list<uint8_t>(bytes, bytes + size));
        this->data_pos += size;
    }

    return copy_size + size;
}

size_t ff::data::data_writer::size() const
{
    return this->data->size();
}

size_t ff::data::data_writer::pos() const
{
    return this->data_pos;
}

size_t ff::data::data_writer::pos(size_t new_pos)
{
    assert(new_pos <= this->size());
    new_pos = std::min(new_pos, this->size());
    this->data_pos = new_pos;
    return new_pos;
}

std::shared_ptr<ff::data::saved_data_base> ff::data::data_writer::saved_data(size_t offset, size_t saved_size, size_t loaded_size, saved_data_type type) const
{
    auto subdata = std::make_shared<data_vector>(this->data, offset, saved_size);
    return std::make_shared<saved_data_static>(subdata, loaded_size, type);
}

ff::data::file_writer::file_writer(file_write&& file)
    : file(std::move(file))
{}

size_t ff::data::file_writer::write(const void* data, size_t size)
{
    return this->file.write(data, size);
}

size_t ff::data::file_writer::size() const
{
    return this->file.size();
}

size_t ff::data::file_writer::pos() const
{
    return this->file.pos();
}

size_t ff::data::file_writer::pos(size_t new_pos)
{
    return this->file.pos(new_pos);
}

std::shared_ptr<ff::data::saved_data_base> ff::data::file_writer::saved_data(size_t offset, size_t saved_size, size_t loaded_size, saved_data_type type) const
{
    assert(offset + saved_size <= this->size());
    return std::make_shared<saved_data_file>(this->file.path(), offset, saved_size, loaded_size, type);
}
