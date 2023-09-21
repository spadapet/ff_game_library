#pragma once

#include "target_base.h"

namespace ff::dxgi
{
    class target_window_base : public ff::dxgi::target_base
    {
    public:
        using ff::dxgi::target_base::size;

        virtual bool size(const ff::window_size& size) = 0;
        virtual ff::signal_sink<ff::window_size>& size_changed() = 0;

        virtual size_t buffer_count() const = 0;
        virtual void buffer_count(size_t value) = 0;
        virtual size_t frame_latency() const = 0;
        virtual void frame_latency(size_t value) = 0;
        virtual bool vsync() const = 0;
        virtual void vsync(bool value) = 0;

        virtual bool allow_full_screen() const = 0;
        virtual bool full_screen() = 0;
        virtual bool full_screen(bool value) = 0;
    };
}
