#pragma once

#include "descriptor_range.h"
#include "mem_range.h"

namespace ff::dx12
{
    class commands;
    class resource;

    /// <summary>
    /// Base class for any type of DX12 buffer resource
    /// </summary>
    class buffer_base : public ff::dxgi::buffer_base, public ff::dx12::residency_access
    {
    public:
        static ff::dx12::buffer_base& get(ff::dxgi::buffer_base& obj);
        static const ff::dx12::buffer_base& get(const ff::dxgi::buffer_base& obj);

        operator bool() const;

        virtual bool valid() const = 0;
        virtual size_t version() const = 0;

        virtual ff::dx12::resource* resource();
        virtual D3D12_VERTEX_BUFFER_VIEW vertex_view(size_t vertex_stride, uint64_t start_offset = 0, size_t vertex_count = 0) const;
        virtual D3D12_INDEX_BUFFER_VIEW index_view(size_t start = 0, size_t count = 0, DXGI_FORMAT format = DXGI_FORMAT_R16_UINT) const;
        virtual D3D12_CPU_DESCRIPTOR_HANDLE constant_view();
        virtual D3D12_GPU_VIRTUAL_ADDRESS gpu_address() const;

        // ff::dx12::residency_access
        virtual ff::dx12::residency_data* residency_data() override;
    };

    /// <summary>
    /// Saves data in GPU default heap, copied from upload heap
    /// </summary>
    class buffer : public ff::dx12::buffer_base, private ff::dxgi::device_child_base
    {
    public:
        buffer(ff::dxgi::buffer_type type, std::shared_ptr<ff::data_base> initial_data = {});
        buffer(buffer&& other) noexcept;
        buffer(const buffer& other) = delete;
        virtual ~buffer() override;

        buffer& operator=(buffer&& other) noexcept = default;
        buffer& operator=(const buffer& other) = delete;

        // ff::dx12::buffer_base
        virtual bool valid() const override;
        virtual size_t version() const override;
        virtual ff::dx12::resource* resource() override;
        virtual D3D12_VERTEX_BUFFER_VIEW vertex_view(size_t vertex_stride, uint64_t start_offset = 0, size_t vertex_count = 0) const override;
        virtual D3D12_INDEX_BUFFER_VIEW index_view(size_t start = 0, size_t count = 0, DXGI_FORMAT format = DXGI_FORMAT_R16_UINT) const override;
        virtual D3D12_CPU_DESCRIPTOR_HANDLE constant_view() override;
        virtual D3D12_GPU_VIRTUAL_ADDRESS gpu_address() const override;

        // ff::dx12::residency_access
        virtual ff::dx12::residency_data* residency_data() override;

        // ff::dxgi::buffer_base
        virtual ff::dxgi::buffer_type type() const override;
        virtual size_t size() const override;
        virtual bool writable() const override;
        virtual bool update(ff::dxgi::command_context_base& context, const void* data, size_t size, size_t min_buffer_size = 0) override;
        virtual void* map(ff::dxgi::command_context_base& context, size_t size) override;
        virtual void unmap() override;

    private:
        buffer(
            ff::dx12::commands* commands,
            ff::dxgi::buffer_type type,
            const void* data,
            uint64_t data_size,
            size_t data_hash,
            size_t version,
            std::shared_ptr<ff::data_base> initial_data,
            std::unique_ptr<std::vector<uint8_t>> mapped_mem = {},
            std::unique_ptr<ff::dx12::resource>&& resource = {},
            uint64_t resource_data_offset = 0);

        // device_child_base
        virtual bool reset() override;

        std::unique_ptr<ff::dx12::resource> resource_;
        std::shared_ptr<ff::data_base> initial_data;
        ff::dx12::descriptor_range constant_view_;
        std::unique_ptr<std::vector<uint8_t>> mapped_mem;
        ff::dxgi::command_context_base* mapped_context;
        ff::dxgi::buffer_type type_;
        uint64_t mem_start;
        uint64_t data_size;
        size_t data_hash;
        size_t version_;
    };

    /// <summary>
    /// Saves data in GPU upload heap
    /// </summary>
    class buffer_upload : public ff::dx12::buffer_base
    {
    public:
        buffer_upload(ff::dxgi::buffer_type type);
        buffer_upload(buffer_upload&& other) noexcept = default;
        buffer_upload(const buffer_upload& other) = delete;

        buffer_upload& operator=(buffer_upload&& other) noexcept = default;
        buffer_upload& operator=(const buffer_upload& other) = delete;

        // ff::dx12::buffer_base
        virtual bool valid() const override;
        virtual size_t version() const override;
        virtual D3D12_VERTEX_BUFFER_VIEW vertex_view(size_t vertex_stride, uint64_t start_offset = 0, size_t vertex_count = 0) const override;
        virtual D3D12_INDEX_BUFFER_VIEW index_view(size_t start = 0, size_t count = 0, DXGI_FORMAT format = DXGI_FORMAT_R16_UINT) const override;
        virtual D3D12_GPU_VIRTUAL_ADDRESS gpu_address() const override;

        // ff::dx12::residency_access
        virtual ff::dx12::residency_data* residency_data() override;

        // ff::dxgi::buffer_base
        virtual ff::dxgi::buffer_type type() const override;
        virtual size_t size() const override;
        virtual bool writable() const override;
        virtual bool update(ff::dxgi::command_context_base& context, const void* data, size_t size, size_t min_buffer_size = 0) override;
        virtual void* map(ff::dxgi::command_context_base& context, size_t size) override;
        virtual void unmap() override;

    private:
        ff::dxgi::buffer_type type_;
        ff::dx12::residency_data* mem_residency_data{};
        D3D12_GPU_VIRTUAL_ADDRESS mem_gpu_address{};
        uint64_t mem_size{};
        size_t version_{};
    };

    /// <summary>
    /// Doesn't upload buffer data to GPU, just saved in CPU vector
    /// </summary>
    class buffer_cpu : public ff::dx12::buffer_base
    {
    public:
        buffer_cpu(ff::dxgi::buffer_type type);
        buffer_cpu(buffer_cpu&& other) noexcept = default;
        buffer_cpu(const buffer_cpu& other) = delete;

        buffer_cpu& operator=(buffer_cpu&& other) noexcept = default;
        buffer_cpu& operator=(const buffer_cpu& other) = delete;

        const std::vector<uint8_t>& data() const;

        // ff::dx12::buffer_base
        virtual bool valid() const override;
        virtual size_t version() const override;

        // ff::dxgi::buffer_base
        virtual ff::dxgi::buffer_type type() const override;
        virtual size_t size() const override;
        virtual bool writable() const override;
        virtual bool update(ff::dxgi::command_context_base& context, const void* data, size_t size, size_t min_buffer_size = 0) override;
        virtual void* map(ff::dxgi::command_context_base& context, size_t size) override;
        virtual void unmap() override;

    private:
        std::vector<uint8_t> data_;
        ff::dxgi::buffer_type type_;
        size_t data_hash;
        size_t version_;
    };
}
