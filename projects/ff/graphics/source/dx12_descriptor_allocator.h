#pragma once

#include "dx12_descriptor_range.h"
#include "graphics_child_base.h"

#if DXVER == 12

namespace ff::internal
{
    class dx12_descriptor_bucket_base
    {
    public:
        virtual ~dx12_descriptor_bucket_base() = default;

        virtual void free_range(const ff::dx12_descriptor_range& range) {}
        virtual D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle(size_t index) const = 0;
        virtual D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle(size_t index) const = 0;
    };

    class dx12_descriptor_bucket : public ff::internal::dx12_descriptor_bucket_base
    {
    public:
        dx12_descriptor_bucket(ID3D12DescriptorHeapX* descriptor_heap, size_t start, size_t count);
        dx12_descriptor_bucket(dx12_descriptor_bucket&& other) noexcept = delete;
        dx12_descriptor_bucket(const dx12_descriptor_bucket& other) = delete;

        dx12_descriptor_bucket& operator=(dx12_descriptor_bucket&& other) noexcept = delete;
        dx12_descriptor_bucket& operator=(const dx12_descriptor_bucket& other) = delete;

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

    class dx12_descriptor_ring : public ff::internal::dx12_descriptor_bucket_base
    {
    public:
        dx12_descriptor_ring(ID3D12DescriptorHeapX* descriptor_heap, size_t start, size_t count);
        dx12_descriptor_ring(dx12_descriptor_ring&& other) noexcept = delete;
        dx12_descriptor_ring(const dx12_descriptor_ring& other) = delete;

        dx12_descriptor_ring& operator=(dx12_descriptor_ring&& other) noexcept = delete;
        dx12_descriptor_ring& operator=(const dx12_descriptor_ring& other) = delete;

        void reset(ID3D12DescriptorHeapX* descriptor_heap);
        ff::dx12_descriptor_range alloc_range(size_t count);
        void fence(uint64_t value);

        virtual D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle(size_t index) const override;
        virtual D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle(size_t index) const override;

    private:
        struct range_t
        {
            size_t after_end() const;

            size_t start;
            size_t count;
            uint64_t fence_value;
        };

        Microsoft::WRL::ComPtr<ID3D12DescriptorHeapX> descriptor_heap;
        std::mutex ranges_mutex;
        std::list<range_t> ranges;
        size_t descriptor_start;
        size_t descriptor_count;
        size_t descriptor_size;
    };
}

namespace ff
{
    class dx12_descriptors_cpu : public ff::internal::graphics_child_base
    {
    public:
        dx12_descriptors_cpu(D3D12_DESCRIPTOR_HEAP_TYPE type, size_t bucket_size);
        dx12_descriptors_cpu(dx12_descriptors_cpu&& other) noexcept = default;
        dx12_descriptors_cpu(const dx12_descriptors_cpu& other) = delete;
        ~dx12_descriptors_cpu();

        dx12_descriptors_cpu& operator=(dx12_descriptors_cpu&& other) noexcept = default;
        dx12_descriptors_cpu& operator=(const dx12_descriptors_cpu& other) = delete;

        ff::dx12_descriptor_range alloc_range(size_t count);

        // graphics_child_base
        virtual bool reset() override;
        virtual int reset_priority() const override;

    private:
        std::mutex bucket_mutex;
        std::list<ff::internal::dx12_descriptor_bucket> buckets;
        D3D12_DESCRIPTOR_HEAP_TYPE type;
        size_t bucket_size;
    };

    class dx12_descriptors_gpu : public ff::internal::graphics_child_base
    {
    public:
        dx12_descriptors_gpu(D3D12_DESCRIPTOR_HEAP_TYPE type, size_t ring_size);
        dx12_descriptors_gpu(dx12_descriptors_gpu&& other) noexcept = default;
        dx12_descriptors_gpu(const dx12_descriptors_gpu& other) = delete;
        ~dx12_descriptors_gpu();

        dx12_descriptors_gpu& operator=(dx12_descriptors_gpu&& other) noexcept = default;
        dx12_descriptors_gpu& operator=(const dx12_descriptors_gpu& other) = delete;

        ID3D12DescriptorHeapX* get() const;
        ff::dx12_descriptor_range alloc_range(size_t count);

        // graphics_child_base
        virtual bool reset() override;
        virtual int reset_priority() const override;

    private:
        void render_frame_complete(uint64_t fence_value);

        Microsoft::WRL::ComPtr<ID3D12DescriptorHeapX> descriptor_heap;
        std::unique_ptr<ff::internal::dx12_descriptor_ring> ring;
        ff::signal_connection render_frame_complete_connection;
        D3D12_DESCRIPTOR_HEAP_TYPE type;
        size_t ring_size;
    };
}

#endif
