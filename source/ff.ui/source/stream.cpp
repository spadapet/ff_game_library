#include "pch.h"
#include "stream.h"

ff::internal::ui::stream::stream(ff::auto_resource_value&& resource)
    : resource_(std::move(resource))
{
    auto resource_file = this->resource_.valid()
        ? std::dynamic_pointer_cast<ff::resource_file>(this->resource_.value()->get<ff::resource_object_base>())
        : nullptr;

    if (resource_file && resource_file->saved_data())
    {
        this->reader_ = resource_file->saved_data()->loaded_reader();
    }

    assert(this->reader_);
}

ff::internal::ui::stream::stream(std::shared_ptr<ff::reader_base> reader)
    : reader_(reader)
{
    assert(this->reader_);
}

void ff::internal::ui::stream::SetPosition(uint32_t pos)
{
    if (this->reader_)
    {
        this->reader_->pos(static_cast<size_t>(pos));
    }
}

uint32_t ff::internal::ui::stream::GetPosition() const
{
    if (this->reader_)
    {
        return static_cast<uint32_t>(this->reader_->pos());
    }

    return 0;
}

uint32_t ff::internal::ui::stream::GetLength() const
{
    if (this->reader_)
    {
        return static_cast<uint32_t>(this->reader_->size());
    }

    return 0;
}

uint32_t ff::internal::ui::stream::Read(void* buffer, uint32_t size)
{
    if (this->reader_)
    {
        size = std::min<uint32_t>(size, this->GetLength() - this->GetPosition());

        if (buffer)
        {
            uint32_t size_read = static_cast<uint32_t>(this->reader_->read(buffer, static_cast<size_t>(size)));
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

void ff::internal::ui::stream::Close()
{
    this->resource_ = ff::auto_resource_value();
    this->reader_.reset();
}
