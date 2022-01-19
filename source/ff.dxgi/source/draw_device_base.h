#pragma once

#include "draw_ptr.h"

namespace ff::dxgi
{
    class depth_base;
    class target_base;

    enum class draw_options
    {
        none = 0x00,
        pre_multiplied_alpha = 0x01,
        ignore_rotation = 0x02,
    };

    class draw_device_base
    {
    public:
        virtual ~draw_device_base() = default;

        virtual bool valid() const = 0;
        virtual ff::dxgi::draw_ptr begin_draw(ff::dxgi::target_base& target, ff::dxgi::depth_base* depth, const ff::rect_float& view_rect, const ff::rect_float& world_rect, ff::dxgi::draw_options options = ff::dxgi::draw_options::none) = 0;
        ff::dxgi::draw_ptr begin_draw(ff::dxgi::target_base& target, ff::dxgi::depth_base* depth, const ff::rect_fixed& view_rect, const ff::rect_fixed& world_rect, ff::dxgi::draw_options options = ff::dxgi::draw_options::none);
    };
}
