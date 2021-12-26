#pragma once

#include "mem_range.h"

namespace ff::dx12
{
    class buffer_upload : public ff::dxgi::buffer_base
    {
    public:
        buffer_upload(ff::dxgi::buffer_type type);
        buffer_upload(buffer_upload&& other) noexcept = default;
        buffer_upload(const buffer_upload& other) = delete;

        buffer_upload& operator=(buffer_upload&& other) noexcept = default;
        buffer_upload& operator=(const buffer_upload& other) = delete;
        operator bool() const;

        D3D12_VERTEX_BUFFER_VIEW vertex_view(size_t vertex_stride, uint64_t start_offset = 0, size_t vertex_count = 0) const;
        D3D12_INDEX_BUFFER_VIEW index_view(size_t start = 0, size_t count = 0) const;
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
        ff::dx12::mem_range mem_range;
        ff::dxgi::buffer_type type_;
        size_t version_;
    };
}
