#pragma once

#include "dx11_target_base.h"

namespace ff
{
    class dx11_target_window_base : public dx11_target_base
    {
    public:
        using dx11_target_base::size;

        virtual bool present(bool vsync) = 0;
        virtual bool size(const ff::window_size& size) = 0;
        virtual ff::signal_sink<ff::window_size>& size_changed() = 0;

        virtual bool allow_full_screen() const = 0;
        virtual bool full_screen() = 0;
        virtual bool full_screen(bool value) = 0;
    };
}
