#pragma once

#include "../dxgi/target_base.h"

namespace ff::dxgi
{
    class target_window_base : public ff::dxgi::target_base
    {
    public:
        using ff::dxgi::target_base::size;

        virtual bool size(const ff::window_size& size) = 0;
        virtual void notify_window_message(ff::window* window, ff::window_message& message) = 0;
        virtual size_t buffer_count() const = 0;
    };
}
