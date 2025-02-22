#pragma once

namespace ff::dxgi
{
    class texture_view_base;

    enum class sprite_type
    {
        unknown = 0x00,
        opaque = 0x01,
        transparent = 0x02,
        palette = 0x10,

        opaque_palette = opaque | palette,
    };

    class sprite_data
    {
    public:
        sprite_data();
        sprite_data(
            ff::dxgi::texture_view_base* view,
            ff::rect_float texture_uv,
            ff::rect_float world,
            ff::dxgi::sprite_type type);
        sprite_data(
            ff::dxgi::texture_view_base* view,
            ff::rect_float rect,
            ff::point_float handle,
            ff::point_float scale,
            ff::dxgi::sprite_type type);
        sprite_data(sprite_data&& other) noexcept = default;
        sprite_data(const sprite_data& other) = default;

        sprite_data& operator=(sprite_data&& other) noexcept = default;
        sprite_data& operator=(const sprite_data & other) = default;
        operator bool() const;

        ff::dxgi::texture_view_base* view() const;
        const ff::rect_float& texture_uv() const;
        const ff::rect_float& world() const;
        ff::dxgi::sprite_type type() const;

        ff::rect_float texture_rect() const;
        ff::point_float scale() const;
        ff::point_float handle() const;

    private:
        ff::dxgi::texture_view_base* view_;
        ff::rect_float texture_uv_;
        ff::rect_float world_;
        ff::dxgi::sprite_type type_;
    };

    ff::dxgi::sprite_type get_sprite_type(const DirectX::ScratchImage& scratch, const ff::rect_size* rect = nullptr);
}
