#pragma once

#include "draw_base.h"
#include "draw_ptr.h"

namespace ff
{
    class depth;
    class target_base;

    enum class draw_options
    {
        none = 0x00,
        pre_multiplied_alpha = 0x01,
    };

    class draw_device
    {
    public:
        static std::unique_ptr<ff::draw_device> create();

        virtual ~draw_device() = default;
        virtual bool valid() const = 0;
        virtual ff::draw_ptr begin_draw(ff::dxgi::target_base& target, ff::dxgi::depth_base* depth, const ff::rect_float& view_rect, const ff::rect_float& world_rect, ff::draw_options options = ff::draw_options::none) = 0;
        ff::draw_ptr begin_draw(ff::dxgi::target_base& target, ff::dxgi::depth_base* depth, const ff::rect_fixed& view_rect, const ff::rect_fixed& world_rect, ff::draw_options options = ff::draw_options::none);
    };
}
