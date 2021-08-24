#pragma once

#include "dx12_descriptor_range.h"
#include "graphics_child_base.h"

#if DXVER == 12

namespace ff::internal
{
    class dx12_descriptor_buffer_base
    {
    public:
        virtual ~dx12_descriptor_buffer_base() = default;

        virtual void free_range(const ff::dx12_descriptor_range& range) {}
        virtual D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle(size_t index) const = 0;
        virtual D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle(size_t index) const = 0;
    };

    class dx12_descriptor_buffer_free_list : public ff::internal::dx12_descriptor_buffer_base
    {
    public:
        dx12_descriptor_buffer_free_list(ID3D12DescriptorHeapX* descriptor_heap, size_t start, size_t count);
        dx12_descriptor_buffer_free_list(dx12_descriptor_buffer_free_list&& other) noexcept = delete;
        dx12_descriptor_buffer_free_list(const dx12_descriptor_buffer_free_list& other) = delete;

        dx12_descriptor_buffer_free_list& operator=(dx12_descriptor_buffer_free_list&& other) noexcept = delete;
        dx12_descriptor_buffer_free_list& operator=(const dx12_descriptor_buffer_free_list& other) = delete;

        ID3D12DescriptorHeapX* get() const;
        void reset(ID3D12DescriptorHeapX* descriptor_heap);
        ff::dx12_descriptor_range alloc_range(size_t count);

        virtual void free_range(const ff::dx12_descriptor_range& range) override;
        virtual D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle(size_t index) const override;
        virtual D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle(size_t index) const override;

    private:
        struct range_t
        {
            bool operator<(const range_t& other) const;
            size_t after_end() const;

            size_t start;
            size_t count;
        };

        Microsoft::WRL::ComPtr<ID3D12DescriptorHeapX> descriptor_heap;
        std::mutex ranges_mutex;
        std::vector<range_t> free_ranges;
        size_t descriptor_start;
        size_t descriptor_count;
        size_t descriptor_size;
    };

    class dx12_descriptor_buffer_ring : public ff::internal::dx12_descriptor_buffer_base
    {
    public:
        dx12_descriptor_buffer_ring(ID3D12DescriptorHeapX* descriptor_heap, size_t start, size_t count);
        dx12_descriptor_buffer_ring(dx12_descriptor_buffer_ring&& other) noexcept = delete;
        dx12_descriptor_buffer_ring(const dx12_descriptor_buffer_ring& other) = delete;

        dx12_descriptor_buffer_ring& operator=(dx12_descriptor_buffer_ring&& other) noexcept = delete;
        dx12_descriptor_buffer_ring& operator=(const dx12_descriptor_buffer_ring& other) = delete;

        ID3D12DescriptorHeapX* get() const;
        void reset(ID3D12DescriptorHeapX* descriptor_heap);
        ff::dx12_descriptor_range alloc_range(size_t count);

        virtual D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle(size_t index) const override;
        virtual D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle(size_t index) const override;

    private:
        void render_frame_complete(uint64_t fence_value);

        struct range_t
        {
            size_t after_end() const;

            size_t start;
            size_t count;
            uint64_t fence_value;
        };

        Microsoft::WRL::ComPtr<ID3D12DescriptorHeapX> descriptor_heap;
        ff::signal_connection render_frame_complete_connection;
        std::mutex ranges_mutex;
        std::list<range_t> ranges;
        size_t descriptor_start;
        size_t descriptor_count;
        size_t descriptor_size;
    };
}

namespace ff
{
    class dx12_descriptor_allocator_base
    {
    public:
        virtual ~dx12_descriptor_allocator_base() = default;

        virtual ff::dx12_descriptor_range alloc_range(size_t count) = 0;
        virtual ff::dx12_descriptor_range alloc_pinned_range(size_t count);
    };

    class dx12_cpu_descriptor_allocator : public ff::dx12_descriptor_allocator_base, public ff::internal::graphics_child_base
    {
    public:
        dx12_cpu_descriptor_allocator(D3D12_DESCRIPTOR_HEAP_TYPE type, size_t bucket_size);
        dx12_cpu_descriptor_allocator(dx12_cpu_descriptor_allocator&& other) noexcept = default;
        dx12_cpu_descriptor_allocator(const dx12_cpu_descriptor_allocator& other) = delete;
        ~dx12_cpu_descriptor_allocator();

        dx12_cpu_descriptor_allocator& operator=(dx12_cpu_descriptor_allocator&& other) noexcept = default;
        dx12_cpu_descriptor_allocator& operator=(const dx12_cpu_descriptor_allocator& other) = delete;

        virtual ff::dx12_descriptor_range alloc_range(size_t count) override;

        // graphics_child_base
        virtual bool reset() override;
        virtual int reset_priority() const override;

    private:
        std::mutex bucket_mutex;
        std::list<ff::internal::dx12_descriptor_buffer_free_list> buckets;
        D3D12_DESCRIPTOR_HEAP_TYPE type;
        size_t bucket_size;
    };

    class dx12_gpu_descriptor_allocator : public ff::dx12_descriptor_allocator_base, public ff::internal::graphics_child_base
    {
    public:
        dx12_gpu_descriptor_allocator(D3D12_DESCRIPTOR_HEAP_TYPE type, size_t pinned_size, size_t ring_size);
        dx12_gpu_descriptor_allocator(dx12_gpu_descriptor_allocator&& other) noexcept = default;
        dx12_gpu_descriptor_allocator(const dx12_gpu_descriptor_allocator& other) = delete;
        ~dx12_gpu_descriptor_allocator();

        dx12_gpu_descriptor_allocator& operator=(dx12_gpu_descriptor_allocator&& other) noexcept = default;
        dx12_gpu_descriptor_allocator& operator=(const dx12_gpu_descriptor_allocator& other) = delete;

        ID3D12DescriptorHeapX* get() const;
        virtual ff::dx12_descriptor_range alloc_range(size_t count) override;
        virtual ff::dx12_descriptor_range alloc_pinned_range(size_t count) override;

        // graphics_child_base
        virtual bool reset() override;
        virtual int reset_priority() const override;

    private:
        std::unique_ptr<ff::internal::dx12_descriptor_buffer_free_list> pinned;
        std::unique_ptr<ff::internal::dx12_descriptor_buffer_ring> ring;
    };
}

#endif
