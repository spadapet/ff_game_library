#pragma once

namespace ff::dx11
{
    class draw_device
    {
    public:
        static std::unique_ptr<ff::dx11::draw_device> create();

        virtual ~draw_device() = default;
        virtual bool valid() const = 0;
        virtual ff::dxgi::draw_ptr begin_draw(ff::dxgi::target_base& target, ff::dxgi::depth_base* depth, const ff::rect_float& view_rect, const ff::rect_float& world_rect, ff::dxgi::draw_options options = ff::dxgi::draw_options::none) = 0;
        ff::dxgi::draw_ptr begin_draw(ff::dxgi::target_base& target, ff::dxgi::depth_base* depth, const ff::rect_fixed& view_rect, const ff::rect_fixed& world_rect, ff::dxgi::draw_options options = ff::dxgi::draw_options::none);
    };
}
