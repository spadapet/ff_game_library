#include "pch.h"
#include "stream.h"

ff::internal::ui::stream::stream(ff::auto_resource_value&& resource)
    : resource(std::move(resource))
{
    auto resource_file = this->resource.valid()
        ? std::dynamic_pointer_cast<ff::resource_file>(this->resource.value()->get<ff::resource_object_base>())
        : nullptr;

    if (resource_file && resource_file->saved_data())
    {
        this->data = resource_file->saved_data()->loaded_data();
        this->reader = std::make_shared<ff::data_reader>(this->data);
    }

    assert(this->reader);
}

ff::internal::ui::stream::stream(std::shared_ptr<ff::reader_base> reader)
    : reader(reader)
{
    assert(this->reader);
}

void ff::internal::ui::stream::SetPosition(uint32_t pos)
{
    if (this->reader)
    {
        this->reader->pos(static_cast<size_t>(pos));
    }
}

uint32_t ff::internal::ui::stream::GetPosition() const
{
    if (this->reader)
    {
        return static_cast<uint32_t>(this->reader->pos());
    }

    return 0;
}

uint32_t ff::internal::ui::stream::GetLength() const
{
    if (this->reader)
    {
        return static_cast<uint32_t>(this->reader->size());
    }

    return 0;
}

uint32_t ff::internal::ui::stream::Read(void* buffer, uint32_t size)
{
    if (this->reader)
    {
        size = std::min<uint32_t>(size, this->GetLength() - this->GetPosition());

        if (buffer)
        {
            uint32_t size_read = static_cast<uint32_t>(this->reader->read(buffer, static_cast<size_t>(size)));
            assert(size_read == size);
            size = size_read;
        }
        else
        {
            this->SetPosition(this->GetPosition() + size);
        }

        return size;
    }

    return 0;
}

const void* ff::internal::ui::stream::GetMemoryBase() const
{
    return this->data ? this->data->data() : nullptr;
}

void ff::internal::ui::stream::Close()
{
    this->resource = ff::auto_resource_value();
    this->reader.reset();
}
