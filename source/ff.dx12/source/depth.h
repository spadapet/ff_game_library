#pragma once

#include "descriptor_range.h"

namespace ff::dx12
{
    class resource;

    class depth : public ff::dxgi::depth_base, private ff::dxgi::device_child_base
    {
    public:
        static const DXGI_FORMAT FORMAT = DXGI_FORMAT_D24_UNORM_S8_UINT;

        depth(size_t sample_count = 0);
        depth(const ff::point_size& size, size_t sample_count = 0);
        depth(depth&& other) noexcept;
        depth(const depth& other) = delete;
        virtual ~depth() override;

        static depth& get(ff::dxgi::depth_base& obj);
        static const depth& get(const ff::dxgi::depth_base& obj);
        depth& operator=(depth&& other) noexcept = default;
        depth& operator=(const depth& other) = delete;
        operator bool() const;

        ff::dx12::resource* resource() const;
        D3D12_CPU_DESCRIPTOR_HANDLE view() const;

        // depth_base
        virtual ff::point_size physical_size() const override;
        virtual bool physical_size(ff::dxgi::command_context_base& context, const ff::point_size& size) override;
        virtual size_t sample_count() const override;
        virtual void clear(ff::dxgi::command_context_base& context, float depth, BYTE stencil) const override;
        virtual void clear_depth(ff::dxgi::command_context_base& context, float depth = 0.0f) const override;
        virtual void clear_stencil(ff::dxgi::command_context_base& context, BYTE stencil) const override;

    private:
        void clear(ff::dx12::commands& commands, const float* depth, const BYTE* stencil) const;

        // device_child_base
        virtual bool reset() override;

        std::unique_ptr<ff::dx12::resource> resource_;
        ff::dx12::descriptor_range view_;
    };
}
