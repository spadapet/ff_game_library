#pragma once

#include "graphics_child_base.h"

#if DXVER == 11

namespace ff
{
    class depth : public ff::internal::graphics_child_base
    {
    public:
        depth(size_t sample_count = 0);
        depth(const ff::point_int& size, size_t sample_count = 0);
        depth(depth&& other) noexcept = default;
        depth(const depth& other) = delete;
        virtual ~depth() override;

        depth& operator=(depth&& other) noexcept = default;
        depth& operator=(const depth& other) = delete;
        operator bool() const;

        ff::point_int size() const;
        bool size(const ff::point_int& size);
        size_t sample_count() const;
        void clear(float depth, BYTE stencil) const;
        void clear_depth(float depth = 0.0f) const;
        void clear_stencil(BYTE stencil) const;
        ID3D11Texture2D* texture() const;
        ID3D11DepthStencilView* view() const;

        // graphics_child_base
        virtual bool reset() override;

    private:
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView> view_;
    };
}

#endif
