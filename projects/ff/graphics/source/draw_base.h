#pragma once

namespace ff
{
    class matrix_stack;
    class palette_base;
    class sprite_data;
    struct pixel_transform;
    struct transform;

    class draw_base
    {
    public:
        using custom_context_func = typename std::function<bool(const std::type_info& vertex_type, bool opaque_only)>;

        virtual ~draw_base() = default;

        virtual void end_draw() = 0;

        virtual void draw_sprite(const ff::sprite_data& sprite, const ff::transform& transform) = 0;
        void draw_sprite(const ff::sprite_data& sprite, const ff::pixel_transform& transform);

        virtual void draw_line_strip(const ff::point_float* points, const DirectX::XMFLOAT4* colors, size_t count, float thickness, bool pixel_thickness = false) = 0;
        virtual void draw_line_strip(const ff::point_float* points, size_t count, const DirectX::XMFLOAT4& color, float thickness, bool pixel_thickness = false) = 0;
        virtual void draw_line(const ff::point_float& start, const ff::point_float& end, const DirectX::XMFLOAT4& color, float thickness, bool pixel_thickness = false) = 0;
        virtual void draw_filled_rectangle(const ff::rect_float& rect, const DirectX::XMFLOAT4* colors) = 0;
        virtual void draw_filled_rectangle(const ff::rect_float& rect, const DirectX::XMFLOAT4& color) = 0;
        virtual void draw_filled_triangles(const ff::point_float* points, const DirectX::XMFLOAT4* colors, size_t count) = 0;
        virtual void draw_filled_circle(const ff::point_float& center, float radius, const DirectX::XMFLOAT4& color) = 0;
        virtual void draw_filled_circle(const ff::point_float& center, float radius, const DirectX::XMFLOAT4& inside_color, const DirectX::XMFLOAT4& outside_color) = 0;
        virtual void draw_outline_rectangle(const ff::rect_float& rect, const DirectX::XMFLOAT4& color, float thickness, bool pixel_thickness = false) = 0;
        virtual void draw_outline_circle(const ff::point_float& center, float radius, const DirectX::XMFLOAT4& color, float thickness, bool pixel_thickness = false) = 0;
        virtual void draw_outline_circle(const ff::point_float& center, float radius, const DirectX::XMFLOAT4& inside_color, const DirectX::XMFLOAT4& outside_color, float thickness, bool pixel_thickness = false) = 0;

        virtual void draw_palette_line_strip(const ff::point_float* points, const int* colors, size_t count, float thickness, bool pixel_thickness = false) = 0;
        virtual void draw_palette_line_strip(const ff::point_float* points, size_t count, int color, float thickness, bool pixel_thickness = false) = 0;
        virtual void draw_palette_line(const ff::point_float& start, const ff::point_float& end, int color, float thickness, bool pixel_thickness = false) = 0;
        virtual void draw_palette_filled_rectangle(const ff::rect_float& rect, const int* colors) = 0;
        virtual void draw_palette_filled_rectangle(const ff::rect_float& rect, int color) = 0;
        virtual void draw_palette_filled_triangles(const ff::point_float* points, const int* colors, size_t count) = 0;
        virtual void draw_palette_filled_circle(const ff::point_float& center, float radius, int color) = 0;
        virtual void draw_palette_filled_circle(const ff::point_float& center, float radius, int inside_color, int outside_color) = 0;
        virtual void draw_palette_outline_rectangle(const ff::rect_float& rect, int color, float thickness, bool pixel_thickness = false) = 0;
        virtual void draw_palette_outline_circle(const ff::point_float& center, float radius, int color, float thickness, bool pixel_thickness = false) = 0;
        virtual void draw_palette_outline_circle(const ff::point_float& center, float radius, int inside_color, int outside_color, float thickness, bool pixel_thickness = false) = 0;

        void draw_line_strip(const ff::point_fixed* points, size_t count, const DirectX::XMFLOAT4& color, ff::fixed_int thickness);
        void draw_line(const ff::point_fixed& start, const ff::point_fixed& end, const DirectX::XMFLOAT4& color, ff::fixed_int thickness);
        void draw_filled_rectangle(const ff::rect_fixed& rect, const DirectX::XMFLOAT4& color);
        void draw_filled_circle(const ff::point_fixed& center, ff::fixed_int radius, const DirectX::XMFLOAT4& color);
        void draw_outline_rectangle(const ff::rect_fixed& rect, const DirectX::XMFLOAT4& color, ff::fixed_int thickness);
        void draw_outline_circle(const ff::point_fixed& center, ff::fixed_int radius, const DirectX::XMFLOAT4& color, ff::fixed_int thickness);

        void draw_palette_line_strip(const ff::point_fixed* points, size_t count, int color, ff::fixed_int thickness);
        void draw_palette_line(const ff::point_fixed& start, const ff::point_fixed& end, int color, ff::fixed_int thickness);
        void draw_palette_filled_rectangle(const ff::rect_fixed& rect, int color);
        void draw_palette_filled_circle(const ff::point_fixed& center, ff::fixed_int radius, int color);
        void draw_palette_outline_rectangle(const ff::rect_fixed& rect, int color, ff::fixed_int thickness);
        void draw_palette_outline_circle(const ff::point_fixed& center, ff::fixed_int radius, int color, ff::fixed_int thickness);

        virtual ff::matrix_stack& world_matrix_stack() = 0;
        virtual void nudge_depth() = 0;

        virtual void push_palette(ff::palette_base* palette) = 0;
        virtual void pop_palette() = 0;
        virtual void push_palette_remap(const uint8_t* remap, size_t hash) = 0;
        virtual void pop_palette_remap() = 0;
        virtual void push_no_overlap() = 0;
        virtual void pop_no_overlap() = 0;
        virtual void push_opaque() = 0;
        virtual void pop_opaque() = 0;
        virtual void push_pre_multiplied_alpha() = 0;
        virtual void pop_pre_multiplied_alpha() = 0;
        virtual void push_custom_context(ff::draw_base::custom_context_func&& func) = 0;
        virtual void pop_custom_context() = 0;
        virtual void push_texture_sampler(D3D11_FILTER filter) = 0;
        virtual void pop_texture_sampler() = 0;
    };
}
