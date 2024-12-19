#pragma once

#include "../dxgi/target_base.h"

namespace ff::dxgi
{
    struct target_window_params
    {
        enum class latency_strategy_t
        {
            before_execute,
            after_execute,
            after_present,

            count
        };

        bool operator==(const target_window_params& other) const = default;

        size_t buffer_count{ 2 };
        size_t frame_latency{ 1 };
        latency_strategy_t latency_strategy{ latency_strategy_t::after_execute };
        bool vsync{ true };
        bool allow_full_screen{ true };
        bool extra_render_target{ true };
    };

    class target_window_base : public ff::dxgi::target_base
    {
    public:
        using ff::dxgi::target_base::size;

        virtual bool size(const ff::window_size& size) = 0;
        virtual ff::signal_sink<ff::window_size>& size_changed() = 0;

        virtual size_t buffer_count() const = 0;
        virtual size_t frame_latency() const = 0;
        virtual bool vsync() const = 0;
        virtual bool allow_full_screen() const = 0;
        virtual bool full_screen(ff::rect_int* windowed_rect = nullptr) = 0;
        virtual bool full_screen(bool value, const ff::rect_int* windowed_rect_override = nullptr) = 0;

        virtual const ff::dxgi::target_window_params& init_params() const = 0;
        virtual void init_params(const ff::dxgi::target_window_params& params) = 0;
    };
}
