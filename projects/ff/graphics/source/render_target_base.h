#pragma once

#include "graphics_child_base.h"

namespace ff
{
    class render_target_base : public ff::internal::graphics_child_base
    {
    public:
        virtual ~render_target_base() = 0;

        virtual DXGI_FORMAT format() const = 0;
        virtual ff::point_int size() const = 0;
        virtual ff::point_int rotated_size() const = 0;
        virtual int rotation() const = 0; // degrees
        virtual double dpi_scale() const = 0;
        virtual void clear(const DirectX::XMFLOAT4* color = nullptr) = 0;
        virtual ID3D11Texture2D* texture() = 0;
        virtual ID3D11RenderTargetView* view() = 0;
    };

    class render_target_swap_chain : public render_target_base
    {
    public:
        virtual bool present(bool vsync) = 0;
        virtual bool size(const ff::window_size& size) = 0;
        virtual ff::signal_sink<void(ff::point_int, double, int)>& size_changed() = 0; // size, dpi_scale, rotation
    };

    class render_target_window : public render_target_swap_chain
    {
    public:
        virtual bool allow_full_screen() const = 0;
        virtual bool full_screen() = 0;
        virtual bool full_screen(bool value) = 0;
    };
}
