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

        virtual bool allow_full_screen() const = 0;
        virtual bool full_screen() = 0;
        virtual bool full_screen(bool value) = 0;
    };
}
