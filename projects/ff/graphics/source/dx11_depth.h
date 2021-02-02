#pragma once

#include "graphics_child_base.h"

namespace ff
{
    class dx11_depth : public ff::internal::graphics_child_base
    {
    public:
        dx11_depth(size_t sample_count = 0);
        dx11_depth(const ff::point_int& size, size_t sample_count = 0);
        dx11_depth(dx11_depth&& other) noexcept = default;
        dx11_depth(const dx11_depth& other) = delete;
        virtual ~dx11_depth() override;

        dx11_depth& operator=(dx11_depth&& other) noexcept = default;
        dx11_depth& operator=(const dx11_depth& other) = delete;
        operator bool() const;

        ff::point_int size() const;
        bool size(const ff::point_int& size);
        size_t sample_count() const;
        void clear(float depth, BYTE stencil) const;
        void clear_depth(float depth = 0.0f) const;
        void clear_stencil(BYTE stencil) const;
        void discard() const;
        ID3D11Texture2D* texture() const;
        ID3D11DepthStencilView* view() const;

        // graphics_child_base
        virtual bool reset() override;

    private:
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView> view_;
    };
}
