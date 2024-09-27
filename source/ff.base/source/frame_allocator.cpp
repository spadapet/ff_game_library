#include "pch.h"
#include "frame_allocator.h"
#include "math.h"

ff::frame_allocator::frame_allocator(size_t size)
{
    size = std::max<size_t>(1024, ff::math::nearest_power_of_two(size));
    this->buffer.end = this->buffer.pos + size;
}

void* ff::frame_allocator::alloc(size_t size, size_t align)
{
    uint8_t* pos = ff::math::align_up(this->buffer.pos, align);
    if (!pos || pos + size > this->buffer.end)
    {
        size_t buffer_size = std::max<size_t>((this->buffer.end - this->buffer.data.get()) * 2, ff::math::nearest_power_of_two(size + align));
        this->temp_buffers.push_back(std::move(this->buffer));

        this->buffer.data.reset(new uint8_t[buffer_size]);
        this->buffer.pos = this->buffer.data.get();
        this->buffer.end = this->buffer.pos + buffer_size;

        pos = ff::math::align_up(this->buffer.pos, align);
    }

    this->buffer.pos = pos + size;
    return pos;
}

void ff::frame_allocator::clear()
{
    this->temp_buffers.clear();
    this->buffer.pos = this->buffer.data.get();
}
