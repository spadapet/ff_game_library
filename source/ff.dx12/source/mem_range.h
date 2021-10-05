#pragma once

namespace ff::dx12
{
    class commands;
    class mem_buffer_base;
    class resource;

    class mem_range
    {
    public:
        mem_range();
        mem_range(ff::dx12::mem_buffer_base& owner, uint64_t start, uint64_t size, uint64_t allocated_start, uint64_t allocated_size);
        mem_range(mem_range&& other) noexcept;
        mem_range(const mem_range& other) = delete;
        ~mem_range();

        mem_range& operator=(mem_range&& other) noexcept;
        mem_range& operator=(const mem_range& other) = delete;

        operator bool() const;
        uint64_t start() const;
        uint64_t size() const;
        uint64_t allocated_start() const;
        uint64_t allocated_size() const;
        void* cpu_data() const;
        ff::dx12::heap* heap() const;

        void active_resource(ff::dx12::resource* resource, ff::dx12::commands* commands);
        ff::dx12::resource* active_resource() const;

        // TODO: Remember what memory is used for each command list

    private:
        void free_range();

        ff::dx12::mem_buffer_base* owner;
        ff::dx12::resource* active_resource_;
        uint64_t start_;
        uint64_t size_;
        uint64_t allocated_start_;
        uint64_t allocated_size_;
    };
}
