#pragma once

#include "../types/color.h"

namespace ff
{
    class matrix_stack;
    struct pixel_transform;
    struct transform;
}

namespace ff::dxgi
{
    class command_context_base;
    class palette_base;
    class sprite_data;
    struct remap_t;

    struct endpoint_t
    {
        ff::point_float pos{};
        ff::color const* color{};
        float size{};
    };

    struct pixel_endpoint_t
    {
        ff::point_fixed pos{};
        ff::color const* color{};
        ff::fixed_int size{};
    };

    class draw_base
    {
    public:
        using custom_context_func = typename std::function<bool(ff::dxgi::command_context_base& context, const std::type_info& vertex_type, bool opaque_only)>;

        virtual ~draw_base() = default;

        virtual void end_draw() = 0;

        // Core drawing
        virtual void draw_sprite(const ff::dxgi::sprite_data& sprite, const ff::transform& transform) = 0;
        virtual void draw_lines(std::span<const ff::dxgi::endpoint_t> points) = 0;
        virtual void draw_triangles(std::span<const ff::dxgi::endpoint_t> points) = 0;
        virtual void draw_rectangle(const ff::rect_float& rect, const ff::color& color, std::optional<float> thickness = std::nullopt) = 0;
        virtual void draw_circle(const ff::dxgi::endpoint_t& pos, std::optional<float> thickness = std::nullopt, const ff::color* outside_color = nullptr) = 0;

        // Pixel drawing, converts to core drawing
        void draw_sprite(const ff::dxgi::sprite_data& sprite, const ff::pixel_transform& transform);
        void draw_lines(std::span<const ff::dxgi::pixel_endpoint_t> points);
        void draw_triangles(std::span<const ff::dxgi::pixel_endpoint_t> points);
        void draw_rectangle(const ff::rect_fixed& rect, const ff::color& color, std::optional<ff::fixed_int> thickness = std::nullopt);
        void draw_circle(const ff::dxgi::pixel_endpoint_t& pos, std::optional<ff::fixed_int> thickness = std::nullopt, const ff::color* outside_color = nullptr);

        // Helpers, converts to core drawing
        void draw_line(const ff::point_float& start, const ff::point_float& end, const ff::color& color, float thickness);

        // Pixel helpers, converts to core drawing
        void draw_line(const ff::point_fixed& start, const ff::point_fixed& end, const ff::color& color, ff::fixed_int thickness);

        virtual ff::matrix_stack& world_matrix_stack() = 0;

        virtual void push_palette(ff::dxgi::palette_base* palette) = 0;
        virtual void pop_palette() = 0;
        virtual void push_palette_remap(ff::dxgi::remap_t remap) = 0;
        virtual void pop_palette_remap() = 0;
        virtual void push_no_overlap() = 0;
        virtual void pop_no_overlap() = 0;
        virtual void push_opaque() = 0;
        virtual void pop_opaque() = 0;
        virtual void push_pre_multiplied_alpha() = 0;
        virtual void pop_pre_multiplied_alpha() = 0;
        virtual void push_custom_context(ff::dxgi::draw_base::custom_context_func&& func) = 0;
        virtual void pop_custom_context() = 0;
        virtual void push_sampler_linear_filter(bool linear_filter) = 0;
        virtual void pop_sampler_linear_filter() = 0;
    };

    using draw_ptr = typename std::unique_ptr<ff::dxgi::draw_base, void(*)(ff::dxgi::draw_base*)>;
}
