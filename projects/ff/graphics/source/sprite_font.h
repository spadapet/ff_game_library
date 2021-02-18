#pragma once

namespace ff
{
    class font_file;
    class draw_base;
    class sprite_list;
    struct transform;

    enum class sprite_font_control : wchar_t
    {
        none = 0xE000,
        text_color, // R, G, B, A
        outline_color, // R, G, B, A
        text_palette_color, // Index
        outline_palette_color, // Index

        after_last
    };

    enum class sprite_font_options
    {
        none,
        no_outline = 0x01, // Only draw text
        no_text = 0x02, // Only draw outline
        no_control = 0x04, // ignore any sprite_font_control chars
    };

    class sprite_font : public ff::resource_object_base
    {
    public:
        sprite_font(const std::shared_ptr<ff::resource>& font_file_resource, float size, int outline_thickness, bool anti_alias);
        sprite_font(const std::shared_ptr<ff::resource>& font_file_resource, float size, int outline_thickness, bool anti_alias,
            const std::shared_ptr<ff::sprite_list>& sprites,
            const std::shared_ptr<ff::sprite_list>& outline_sprites,
            const std::shared_ptr<ff::data_base>& glyphs_data);
        sprite_font(sprite_font&& other) noexcept = default;
        sprite_font(const sprite_font& other) = delete;

        sprite_font& operator=(sprite_font&& other) noexcept = default;
        sprite_font& operator=(const sprite_font & other) = delete;
        operator bool() const;

        virtual ff::point_float draw_text(ff::draw_base* draw, std::string_view text, const ff::transform& transform, const DirectX::XMFLOAT4& outline_color, ff::sprite_font_options options) const;
        virtual ff::point_float measure_text(std::string_view text, ff::point_float scale) const;
        virtual float line_spacing() const;

        virtual bool resource_load_complete(bool from_source) override;
        virtual std::vector<std::shared_ptr<resource>> resource_get_dependencies() const override;

    protected:
        virtual bool save_to_cache(ff::dict& dict, bool& allow_compress) const override;

    private:
        bool init_sprites();
        ff::point_float internal_draw_text(ff::draw_base* draw, const ff::sprite_list* sprites, std::wstring_view text, const ff::transform& transform, ff::sprite_font_options options) const;

        static const size_t MAX_GLYPH_COUNT = 0x10000;

        struct char_and_glyph_info
        {
            uint16_t glyph_to_sprite;
            uint16_t char_to_glyph;
            float glyph_width;
        };

        std::shared_ptr<ff::sprite_list> sprites;
        std::shared_ptr<ff::sprite_list> outline_sprites;
        std::array<char_and_glyph_info, ff::sprite_font::MAX_GLYPH_COUNT> glyphs;

        ff::auto_resource<ff::font_file> font_file_resource;
        std::shared_ptr<ff::font_file> font_file;
        float size;
        int outline_thickness;
        bool anti_alias;
    };
}

namespace ff::internal
{
    class sprite_font_factory : public ff::resource_object_factory<sprite_font>
    {
    public:
        using ff::resource_object_factory<sprite_font>::resource_object_factory;

        virtual std::shared_ptr<resource_object_base> load_from_source(const ff::dict& dict, resource_load_context& context) const override;
        virtual std::shared_ptr<resource_object_base> load_from_cache(const ff::dict& dict) const override;
    };
}
