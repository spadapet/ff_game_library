#pragma once

#include "access.h"
#include "descriptor_range.h"
#include "fence_value.h"

namespace ff::dx12
{
    class descriptor_buffer_base
    {
    public:
        virtual ~descriptor_buffer_base() = default;

        virtual void free_range(const ff::dx12::descriptor_range& range) = 0;
        virtual D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle(size_t index) const = 0;
        virtual D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle(size_t index) const = 0;
    };

    class descriptor_buffer_free_list : public ff::dx12::descriptor_buffer_base
    {
    public:
        descriptor_buffer_free_list(ID3D12DescriptorHeap* descriptor_heap, size_t start, size_t count);
        descriptor_buffer_free_list(descriptor_buffer_free_list&& other) noexcept = delete;
        descriptor_buffer_free_list(const descriptor_buffer_free_list& other) = delete;
        virtual ~descriptor_buffer_free_list() override;

        descriptor_buffer_free_list& operator=(descriptor_buffer_free_list&& other) noexcept = delete;
        descriptor_buffer_free_list& operator=(const descriptor_buffer_free_list& other) = delete;

        D3D12_DESCRIPTOR_HEAP_DESC set(ID3D12DescriptorHeap* descriptor_heap);
        ff::dx12::descriptor_range alloc_range(size_t count);

        virtual void free_range(const ff::dx12::descriptor_range& range) override;
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

        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptor_heap;
        std::mutex ranges_mutex;
        std::vector<range_t> free_ranges;
        size_t descriptor_start;
        size_t descriptor_count;
        size_t descriptor_size;
    };

    class descriptor_buffer_ring : public ff::dx12::descriptor_buffer_base
    {
    public:
        descriptor_buffer_ring(ID3D12DescriptorHeap* descriptor_heap, size_t start, size_t count);
        descriptor_buffer_ring(descriptor_buffer_ring&& other) noexcept = delete;
        descriptor_buffer_ring(const descriptor_buffer_ring& other) = delete;
        virtual ~descriptor_buffer_ring() override;

        descriptor_buffer_ring& operator=(descriptor_buffer_ring&& other) noexcept = delete;
        descriptor_buffer_ring& operator=(const descriptor_buffer_ring& other) = delete;

        void set(ID3D12DescriptorHeap* descriptor_heap);
        ff::dx12::descriptor_range alloc_range(size_t count, ff::dx12::fence_value fence_value);

        virtual void free_range(const ff::dx12::descriptor_range& range) override;
        virtual D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle(size_t index) const override;
        virtual D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle(size_t index) const override;

    private:
        struct range_t
        {
            size_t after_end() const;

            size_t start;
            size_t count;
            ff::dx12::fence_value fence_value;
        };

        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptor_heap;
        std::mutex ranges_mutex;
        std::list<range_t> ranges;
        std::atomic_size_t allocated_range_count;
        size_t descriptor_start;
        size_t descriptor_count;
        size_t descriptor_size;
    };

    class cpu_descriptor_allocator : private ff::dxgi::device_child_base
    {
    public:
        cpu_descriptor_allocator(D3D12_DESCRIPTOR_HEAP_TYPE type, size_t bucket_size);
        cpu_descriptor_allocator(cpu_descriptor_allocator&& other) noexcept = delete;
        cpu_descriptor_allocator(const cpu_descriptor_allocator& other) = delete;
        ~cpu_descriptor_allocator();

        cpu_descriptor_allocator& operator=(cpu_descriptor_allocator&& other) noexcept = delete;
        cpu_descriptor_allocator& operator=(const cpu_descriptor_allocator& other) = delete;

        ff::dx12::descriptor_range alloc_range(size_t count);

    private:
        // device_child_base
        virtual void* before_reset(ff::frame_allocator& allocator) override;
        virtual bool reset(void* data) override;

        std::mutex bucket_mutex;
        std::list<ff::dx12::descriptor_buffer_free_list> buckets;
        D3D12_DESCRIPTOR_HEAP_TYPE type;
        size_t bucket_size;
    };

    class gpu_descriptor_allocator : private ff::dxgi::device_child_base
    {
    public:
        gpu_descriptor_allocator(D3D12_DESCRIPTOR_HEAP_TYPE type, size_t pinned_size, size_t ring_size);
        gpu_descriptor_allocator(gpu_descriptor_allocator&& other) noexcept;
        gpu_descriptor_allocator(const gpu_descriptor_allocator& other) = delete;
        ~gpu_descriptor_allocator();

        gpu_descriptor_allocator& operator=(gpu_descriptor_allocator&& other) noexcept = default;
        gpu_descriptor_allocator& operator=(const gpu_descriptor_allocator& other) = delete;

        ff::dx12::descriptor_range alloc_range(size_t count, ff::dx12::fence_value fence_value);
        ff::dx12::descriptor_range alloc_pinned_range(size_t count);

    private:
        friend ID3D12DescriptorHeap* ff::dx12::get_descriptor_heap(const ff::dx12::gpu_descriptor_allocator& obj);

        // device_child_base
        virtual void* before_reset(ff::frame_allocator& allocator) override;
        virtual bool reset(void* data) override;

        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptor_heap;
        std::unique_ptr<ff::dx12::descriptor_buffer_free_list> pinned;
        std::unique_ptr<ff::dx12::descriptor_buffer_ring> ring;
    };
}
