#pragma once

#include "render_target_base.h"

namespace ff
{
    class render_target_window_base : public render_target_base
    {
    public:
        virtual bool present(bool vsync) = 0;
        virtual bool size(const ff::window_size& size) = 0;
        virtual ff::signal_sink<void(ff::window_size)>& size_changed() = 0;

        virtual bool allow_full_screen() const = 0;
        virtual bool full_screen() = 0;
        virtual bool full_screen(bool value) = 0;
    };
}
