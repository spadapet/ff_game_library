#pragma once

namespace ff::dxgi
{
    class command_context;

    enum class buffer_type
    {
        none,
        vertex,
        index,
        constant,
    };

    class buffer_base
    {
    public:
        virtual ~buffer_base() = default;

        virtual ff::dxgi::buffer_type type() const = 0;
        virtual uint64_t size() const = 0;
        virtual bool writable() const = 0;
        virtual void* map(ff::dxgi::command_context& context, size_t size) = 0;
        virtual void unmap() = 0;
        virtual bool update_discard(ff::dxgi::command_context& context, const void* data, size_t size) = 0;
    };
}
