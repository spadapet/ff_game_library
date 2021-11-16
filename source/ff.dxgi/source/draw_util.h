#pragma once

#include "palette_base.h"

namespace ff::dxgi
{
    class buffer_base;
    class command_context_base;
    class sprite_data;
}

namespace ff::dxgi::draw_util
{
    constexpr size_t MAX_TEXTURES = 32;
    constexpr size_t MAX_TEXTURES_USING_PALETTE = 32;
    constexpr size_t MAX_PALETTES = 128; // 256 color palettes only
    constexpr size_t MAX_PALETTE_REMAPS = 128; // 256 entries only
    constexpr size_t MAX_TRANSFORM_MATRIXES = 1024;
    constexpr size_t MAX_RENDER_COUNT = 524288; // 0x00080000
    constexpr float MAX_RENDER_DEPTH = 1.0f;
    constexpr float RENDER_DEPTH_DELTA = MAX_RENDER_DEPTH / MAX_RENDER_COUNT;

    const std::array<uint8_t, ff::dxgi::palette_size>& default_palette_remap();
    size_t default_palette_remap_hash();

    enum class alpha_type
    {
        opaque,
        transparent,
        invisible,
    };

    enum class last_depth_type
    {
        none,
        nudged,

        line,
        circle,
        triangle,
        sprite,

        line_no_overlap,
        circle_no_overlap,
        triangle_no_overlap,
        sprite_no_overlap,

        start_no_overlap = line_no_overlap,
    };

    enum class geometry_bucket_type
    {
        lines,
        circles,
        triangles,
        sprites,
        palette_sprites,

        lines_alpha,
        circles_alpha,
        triangles_alpha,
        sprites_alpha,

        count,
        first_alpha = lines_alpha,
    };

    class geometry_bucket
    {
    protected:
        geometry_bucket(ff::dxgi::draw_util::geometry_bucket_type bucket_type, const std::type_info& item_type, size_t item_size, size_t item_align);
        geometry_bucket(ff::dxgi::draw_util::geometry_bucket&& rhs) noexcept;

    public:
        virtual ~geometry_bucket();

        void reset(std::string_view vs_res, std::string_view gs_res, std::string_view ps_res, std::string_view ps_palette_out_res);
        virtual void reset();
        virtual void apply(ff::dxgi::command_context_base& context, ff::dxgi::buffer_base& geometry_buffer, bool palette_out) const = 0;

        void* add(const void* data = nullptr);
        size_t item_size() const;
        const std::type_info& item_type() const;
        ff::dxgi::draw_util::geometry_bucket_type bucket_type() const;
        size_t count() const;
        void clear_items();
        size_t byte_size() const;
        const uint8_t* data() const;
        void render_start(size_t start);
        size_t render_start() const;
        size_t render_count() const;

    protected:
        std::string vs_res_name;
        std::string gs_res_name;
        std::string ps_res_name;
        std::string ps_palette_out_res_name;

    private:
        ff::dxgi::draw_util::geometry_bucket_type bucket_type_;
        const std::type_info* item_type_;
        size_t item_size_;
        size_t item_align;
        size_t render_start_;
        size_t render_count_;
        uint8_t* data_start;
        uint8_t* data_cur;
        uint8_t* data_end;
    };

    ff::dxgi::draw_util::alpha_type get_alpha_type(const DirectX::XMFLOAT4& color, bool force_opaque);
    ff::dxgi::draw_util::alpha_type get_alpha_type(const DirectX::XMFLOAT4* colors, size_t count, bool force_opaque);
    ff::dxgi::draw_util::alpha_type get_alpha_type(const ff::dxgi::sprite_data& data, const DirectX::XMFLOAT4& color, bool force_opaque);
    ff::dxgi::draw_util::alpha_type get_alpha_type(const ff::dxgi::sprite_data** datas, const DirectX::XMFLOAT4* colors, size_t count, bool force_opaque);
}
