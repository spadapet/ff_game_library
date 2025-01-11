#pragma once

namespace ff::dxgi
{
    class command_context_base;

    enum class buffer_type
    {
        none,
        vertex,
        index,
        constant,
    };

    std::string_view buffer_type_name(ff::dxgi::buffer_type type);

    class buffer_base
    {
    public:
        virtual ~buffer_base() = default;

        virtual ff::dxgi::buffer_type type() const = 0;
        virtual size_t size() const = 0;
        virtual bool writable() const = 0;
        virtual bool update(ff::dxgi::command_context_base& context, const void* data, size_t size, size_t min_buffer_size = 0) = 0;
        virtual void* map(ff::dxgi::command_context_base& context, size_t size) = 0;
        virtual void unmap() = 0;
    };
}
