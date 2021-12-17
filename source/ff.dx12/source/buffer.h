#pragma once

#include "descriptor_range.h"

namespace ff::dx12
{
    class commands;
    class resource;

    class buffer : public ff::dxgi::buffer_base, private ff::dxgi::device_child_base
    {
    public:
        buffer(ff::dxgi::buffer_type type, std::shared_ptr<ff::data_base> initial_data = {});
        buffer(buffer&& other) noexcept = default;
        buffer(const buffer& other) = delete;
        virtual ~buffer() override;

        static ff::dx12::buffer& get(ff::dxgi::buffer_base& obj);
        static const ff::dx12::buffer& get(const ff::dxgi::buffer_base& obj);
        buffer& operator=(buffer&& other) noexcept = default;
        buffer& operator=(const buffer & other) = delete;
        operator bool() const;

        ff::dx12::resource* resource();
        D3D12_VERTEX_BUFFER_VIEW vertex_view(size_t vertex_stride, uint64_t start_offset = 0, size_t vertex_count = 0) const;
        D3D12_INDEX_BUFFER_VIEW index_view(size_t start = 0, size_t count = 0) const;
        D3D12_CPU_DESCRIPTOR_HANDLE constant_view() const;
        D3D12_GPU_VIRTUAL_ADDRESS gpu_address() const;
        size_t version() const;

        // buffer_base
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
            std::unique_ptr<std::vector<uint8_t>> mapped_mem);

        // device_child_base
        virtual bool reset() override;

        std::unique_ptr<ff::dx12::resource> resource_;
        std::shared_ptr<ff::data_base> initial_data;
        ff::dx12::descriptor_range constant_view_;
        std::unique_ptr<std::vector<uint8_t>> mapped_mem;
        ff::dxgi::command_context_base* mapped_context;
        ff::dxgi::buffer_type type_;
        uint64_t data_size;
        size_t data_hash;
        size_t version_;
    };
}
