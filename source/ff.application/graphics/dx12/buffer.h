#pragma once

#include "../dx12/descriptor_range.h"
#include "../dx12/mem_range.h"
#include "../dx12/resource.h"
#include "../dxgi/buffer_base.h"
#include "../dxgi/device_child_base.h"

namespace ff::dx12
{
    /// <summary>
    /// Base class for any type of DX12 buffer resource
    /// </summary>
    class buffer_base : public ff::dxgi::buffer_base, public ff::dx12::residency_access
    {
    public:
        operator bool() const;

        virtual bool valid() const = 0;
        virtual size_t version() const;
        virtual ff::dx12::resource* resource();
        virtual D3D12_GPU_VIRTUAL_ADDRESS gpu_address() const;

        D3D12_VERTEX_BUFFER_VIEW vertex_view(size_t vertex_stride, uint64_t start_offset = 0, size_t vertex_count = 0) const;
        D3D12_INDEX_BUFFER_VIEW index_view(DXGI_FORMAT format = DXGI_FORMAT_R16_UINT, size_t start = 0, size_t count = 0) const;

        // ff::dxgi::buffer_base
        virtual ff::dxgi::buffer_type type() const override;
        virtual bool writable() const override;
        virtual bool update(ff::dxgi::command_context_base& context, const void* data, size_t size) override;
        virtual void* map(ff::dxgi::command_context_base& context, size_t size) override;
        virtual void unmap(ff::dxgi::command_context_base& context) override;

        // ff::dx12::residency_access
        virtual ff::dx12::residency_data* residency_data() override;

    protected:
        buffer_base(ff::dxgi::buffer_type type);
        buffer_base(buffer_base&& other) = default;
        buffer_base(buffer_base& other) = delete;

        buffer_base& operator=(buffer_base&& other) noexcept = default;
        buffer_base& operator=(const buffer_base& other) = delete;

    private:
        ff::dxgi::buffer_type type_;
    };

    /// <summary>
    /// Allocates static GPU memory and copies static data into it
    /// </summary>
    class buffer_gpu_static : public ff::dx12::buffer_base, private ff::dxgi::device_child_base
    {
    public:
        buffer_gpu_static(ff::dxgi::buffer_type type, std::shared_ptr<ff::data_base> static_data);
        buffer_gpu_static(buffer_gpu_static&& other) noexcept;
        buffer_gpu_static(const buffer_gpu_static& other) = delete;
        virtual ~buffer_gpu_static() override;

        buffer_gpu_static& operator=(buffer_gpu_static&& other) noexcept = default;
        buffer_gpu_static& operator=(const buffer_gpu_static& other) = delete;

        // ff::dx12::buffer_base
        virtual bool valid() const override;
        virtual ff::dx12::resource* resource() override;
        virtual D3D12_GPU_VIRTUAL_ADDRESS gpu_address() const override;

        // ff::dxgi::buffer_base
        virtual size_t size() const override;

        // ff::dx12::residency_access
        virtual ff::dx12::residency_data* residency_data() override;

    private:
        // ff::dxgi::device_child_base
        virtual bool reset() override;

        std::shared_ptr<ff::data_base> static_data;
        ff::dx12::resource resource_;
    };

    /// <summary>
    /// Saves data in GPU default heap, copied from upload heap
    /// </summary>
    class buffer_gpu : public ff::dx12::buffer_base
    {
    public:
        buffer_gpu(ff::dxgi::buffer_type type);
        buffer_gpu(buffer_gpu&& other) noexcept = default;
        buffer_gpu(const buffer_gpu& other) = delete;

        buffer_gpu& operator=(buffer_gpu&& other) noexcept = default;
        buffer_gpu& operator=(const buffer_gpu& other) = delete;

        // ff::dx12::buffer_base
        virtual bool valid() const override;
        virtual size_t version() const override;
        virtual ff::dx12::resource* resource() override;
        virtual D3D12_GPU_VIRTUAL_ADDRESS gpu_address() const override;

        // ff::dx12::residency_access
        virtual ff::dx12::residency_data* residency_data() override;

        // ff::dxgi::buffer_base
        virtual size_t size() const override;
        virtual bool writable() const override;
        virtual bool update(ff::dxgi::command_context_base& context, const void* data, size_t size) override;
        virtual void* map(ff::dxgi::command_context_base& context, size_t size) override;
        virtual void unmap(ff::dxgi::command_context_base& context) override;

    private:
        std::unique_ptr<ff::dx12::resource> resource_;
        ff::dx12::mem_range mapped_memory;
        size_t data_hash{};
        size_t version_{ 1 };
    };

    /// <summary>
    /// Doesn't upload buffer data to GPU, just saved in CPU vector
    /// </summary>
    class buffer_cpu : public ff::dx12::buffer_base, public ff::data_base
    {
    public:
        buffer_cpu(ff::dxgi::buffer_type type);
        buffer_cpu(buffer_cpu&& other) noexcept = default;
        buffer_cpu(const buffer_cpu& other) = delete;

        buffer_cpu& operator=(buffer_cpu&& other) noexcept = default;
        buffer_cpu& operator=(const buffer_cpu& other) = delete;

        // ff::data_base
        virtual const uint8_t* data() const override;
        virtual std::shared_ptr<data_base> subdata(size_t offset, size_t size) const override;

        // ff::dx12::buffer_base
        virtual bool valid() const override;
        virtual size_t version() const override;

        // ff::dxgi::buffer_base
        virtual size_t size() const override;
        virtual bool writable() const override;
        virtual bool update(ff::dxgi::command_context_base& context, const void* data, size_t size) override;
        virtual void* map(ff::dxgi::command_context_base& context, size_t size) override;
        virtual void unmap(ff::dxgi::command_context_base& context) override;

    private:
        std::vector<uint8_t> data_;
        size_t data_hash{};
        size_t version_{ 1 };
    };
}
