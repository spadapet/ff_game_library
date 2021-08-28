#pragma once

#include "graphics_child_base.h"

#if DXVER == 12

namespace ff
{
    class dx12_heap : public ff::internal::graphics_child_base
    {
    public:
        enum class usage_t
        {
            upload,
            gpu_buffers,
            gpu_textures,
        };

        dx12_heap(size_t size, ff::dx12_heap::usage_t usage);
        dx12_heap(dx12_heap&& other) noexcept = default;
        dx12_heap(const dx12_heap& other) = delete;
        ~dx12_heap();

        dx12_heap& operator=(dx12_heap&& other) noexcept = default;
        dx12_heap& operator=(const dx12_heap& other) = delete;

        operator bool() const;
        ID3D12HeapX* get() const;
        ID3D12HeapX* operator->() const;
        void used_this_frame();
        size_t size() const;
        usage_t usage() const;

        // graphics_child_base
        virtual bool reset() override;
        virtual int reset_priority() const override;

    private:
        void render_frame_complete(uint64_t fence_value);

        ff::signal_connection render_frame_complete_connection;
        Microsoft::WRL::ComPtr<ID3D12HeapX> heap;
        size_t last_used_frame;
        size_t size_;
        usage_t usage_;
        bool evicted;
    };
}

#endif
