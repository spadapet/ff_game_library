#pragma once

#include "fence_value.h"
#include "heap.h"
#include "mem_range.h"

namespace ff::internal::dx12
{
    class mem_buffer_base
    {
    public:
        virtual ~mem_buffer_base() = default;

        // Called from mem_range
        virtual void free_range(const ff::dx12::mem_range& range) = 0;
        virtual void* cpu_data(uint64_t start);

        // Called from mem_allocator_base
        virtual ff::dx12::heap& heap() = 0;
        virtual bool frame_complete() = 0;
        virtual ff::dx12::mem_range alloc_bytes(uint64_t size, uint64_t align, ff::dx12::fence_value fence_value) = 0;
    };

    class mem_buffer_ring : public ff::internal::dx12::mem_buffer_base
    {
    public:
        mem_buffer_ring(uint64_t size, ff::dx12::heap::usage_t usage);
        mem_buffer_ring(mem_buffer_ring&& other) noexcept = delete;
        mem_buffer_ring(const mem_buffer_ring& other) = delete;
        virtual ~mem_buffer_ring() override;

        mem_buffer_ring& operator=(mem_buffer_ring&& other) noexcept = delete;
        mem_buffer_ring& operator=(const mem_buffer_ring& other) = delete;

        // mem_buffer_base
        virtual void free_range(const ff::dx12::mem_range& range);
        virtual void* cpu_data(uint64_t start) override;
        virtual ff::dx12::heap& heap() override;
        virtual bool frame_complete() override;
        virtual ff::dx12::mem_range alloc_bytes(uint64_t size, uint64_t align, ff::dx12::fence_value fence_value) override;

    private:
        struct range_t
        {
            uint64_t after_end() const;

            uint64_t start;
            uint64_t size;
            ff::dx12::fence_value fence_value;
        };

        ff::dx12::heap heap_;
        std::list<range_t> ranges;
        std::atomic_size_t allocated_range_count;
    };

    class mem_buffer_free_list : public ff::internal::dx12::mem_buffer_base
    {
    public:
        mem_buffer_free_list(uint64_t size, ff::dx12::heap::usage_t usage);
        mem_buffer_free_list(mem_buffer_free_list&& other) noexcept = delete;
        mem_buffer_free_list(const mem_buffer_free_list& other) = delete;
        virtual ~mem_buffer_free_list() override;

        mem_buffer_free_list& operator=(mem_buffer_free_list&& other) noexcept = delete;
        mem_buffer_free_list& operator=(const mem_buffer_free_list& other) = delete;

        // mem_buffer_base
        virtual void free_range(const ff::dx12::mem_range& range);
        virtual void* cpu_data(uint64_t start) override;
        virtual ff::dx12::heap& heap() override;
        virtual bool frame_complete() override;
        virtual ff::dx12::mem_range alloc_bytes(uint64_t size, uint64_t align, ff::dx12::fence_value fence_value) override;

    private:
        struct range_t
        {
            bool operator<(const range_t& other) const;
            uint64_t after_end() const;

            uint64_t start;
            uint64_t size;
        };

        ff::dx12::heap heap_;
        std::mutex ranges_mutex;
        std::vector<range_t> free_ranges;
    };
}

namespace ff::dx12
{
    class mem_allocator_base
    {
    public:
        virtual ~mem_allocator_base() = default;

    protected:
        mem_allocator_base(uint64_t initial_size, uint64_t max_size, ff::dx12::heap::usage_t usage);

        ff::dx12::heap::usage_t usage() const;
        ff::dx12::mem_range alloc_bytes(uint64_t size, uint64_t align, ff::dx12::fence_value fence_value);
        virtual std::unique_ptr<ff::internal::dx12::mem_buffer_base> new_buffer(uint64_t size, ff::dx12::heap::usage_t usage) const = 0;

    private:
        void frame_complete(size_t frame_count);

        std::mutex buffers_mutex;
        std::vector<std::unique_ptr<ff::internal::dx12::mem_buffer_base>> buffers;
        ff::signal_connection frame_complete_connection;
        ff::dx12::heap::usage_t usage_;
        uint64_t initial_size;
        uint64_t max_size;
    };

    // For transient allocations used in a single frame: Upload buffers, vertexes, or constants
    class mem_allocator_ring : public ff::dx12::mem_allocator_base
    {
    public:
        mem_allocator_ring(uint64_t initial_size, ff::dx12::heap::usage_t usage);
        mem_allocator_ring(mem_allocator_ring&& other) noexcept = default;
        mem_allocator_ring(const mem_allocator_ring& other) = delete;

        mem_allocator_ring& operator=(mem_allocator_ring&& other) noexcept = default;
        mem_allocator_ring& operator=(const mem_allocator_ring& other) = delete;

        ff::dx12::mem_range alloc_buffer(uint64_t size, ff::dx12::fence_value fence_value);
        ff::dx12::mem_range alloc_texture(uint64_t size, ff::dx12::fence_value fence_value);

    protected:
        virtual std::unique_ptr<ff::internal::dx12::mem_buffer_base> new_buffer(uint64_t size, ff::dx12::heap::usage_t usage) const override;
    };

    // For long term constants, vertices, and textures
    class mem_allocator : public ff::dx12::mem_allocator_base
    {
    public:
        mem_allocator(uint64_t initial_size, uint64_t max_size, ff::dx12::heap::usage_t usage);
        mem_allocator(mem_allocator&& other) noexcept = default;
        mem_allocator(const mem_allocator& other) = delete;

        mem_allocator& operator=(mem_allocator&& other) noexcept = default;
        mem_allocator& operator=(const mem_allocator& other) = delete;

        ff::dx12::mem_range alloc_bytes(uint64_t size, uint64_t align = 0);

    protected:
        virtual std::unique_ptr<ff::internal::dx12::mem_buffer_base> new_buffer(uint64_t size, ff::dx12::heap::usage_t usage) const override;
    };
}
