#include "pch.h"
#include "color.h"
#include "draw_base.h"
#include "dx11_texture.h"
#include "font_file.h"
#include "graphics.h"
#include "sprite.h"
#include "sprite_font.h"
#include "sprite_list.h"
#include "sprite_optimizer.h"
#include "transform.h"

static bool text_contains_outline_control(std::wstring_view text)
{
    static const std::wstring controls =
    {
        static_cast<wchar_t>(ff::sprite_font_control::outline_color),
        static_cast<wchar_t>(ff::sprite_font_control::outline_palette_color),
    };

    return text.find_first_of(controls) != std::wstring_view::npos;
}

static std::wstring_view to_wstring(std::string_view text, std::array<wchar_t, 2048>& wtext_array, std::wstring& wtext_string)
{
    if (!text.empty())
    {
        int wcount = ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wtext_array.data(), static_cast<int>(wtext_array.size()));
        if (wcount)
        {
            return std::wstring_view(wtext_array.data(), static_cast<size_t>(wcount));
        }
        else if (::GetLastError() == ERROR_INSUFFICIENT_BUFFER)
        {
            wtext_string = ff::string::to_wstring(text);
            return wtext_string;
        }
        else
        {
            assert(false);
        }
    }

    return L"";
}

ff::sprite_font::sprite_font(const std::shared_ptr<ff::resource>& font_file_resource, float size, int outline_thickness, bool anti_alias)
    : glyphs{}
    , font_file_resource(font_file_resource)
    , size(size)
    , outline_thickness(outline_thickness)
    , anti_alias(anti_alias)
{}

ff::sprite_font::sprite_font(
    const std::shared_ptr<ff::resource>& font_file_resource,
    float size,
    int outline_thickness,
    bool anti_alias,
    const std::shared_ptr<ff::sprite_list>& sprites,
    const std::shared_ptr<ff::sprite_list>& outline_sprites,
    const std::shared_ptr<ff::data_base>& glyphs_data)
    : sprites(sprites)
    , outline_sprites(outline_sprites)
    , glyphs{}
    , font_file_resource(font_file_resource)
    , size(size)
    , outline_thickness(outline_thickness)
    , anti_alias(anti_alias)
{
    assert(glyphs_data && glyphs_data->size() == ff::array_byte_size(this->glyphs));
    std::memcpy(this->glyphs.data(), glyphs_data->data(), std::min(this->glyphs.size(), glyphs_data->size()));
}

ff::sprite_font::operator bool() const
{
    return this->font_file && this->sprites;
}

ff::point_float ff::sprite_font::draw_text(
    ff::draw_base* draw,
    std::string_view text,
    const ff::transform& transform,
    const DirectX::XMFLOAT4& outline_color,
    ff::sprite_font_options options) const
{
    std::array<wchar_t, 2048> wtext_array;
    std::wstring wtext_string;
    std::wstring_view wtext = ::to_wstring(text, wtext_array, wtext_string);
    ff::point_float size{};

    if ((outline_color.w > 0 || ::text_contains_outline_control(wtext)) && !ff::flags::has(options, ff::sprite_font_options::no_outline) && this->outline_sprites)
    {
        ff::transform outline_transform = transform;
        outline_transform.color = outline_color;
        size = this->internal_draw_text(draw, this->outline_sprites.get(), wtext, outline_transform, options);
        draw->nudge_depth();
    }

    if (!ff::flags::has(options, ff::sprite_font_options::no_text))
    {
        size = this->internal_draw_text(draw, this->sprites.get(), wtext, transform, options);
    }

    return size;
}

ff::point_float ff::sprite_font::measure_text(std::string_view text, ff::point_float scale) const
{
    std::array<wchar_t, 2048> wtext_array;
    std::wstring wtext_string;
    std::wstring_view wtext = ::to_wstring(text, wtext_array, wtext_string);

    return this->internal_draw_text(nullptr, nullptr, wtext, ff::transform::identity(), ff::sprite_font_options::no_control);
}

float ff::sprite_font::line_spacing() const
{
    IDWriteFontFaceX* font_face = this->font_file ? this->font_file->font_face() : nullptr;
    assert(font_face);

    if (font_face)
    {
        DWRITE_FONT_METRICS1 fm;
        font_face->GetMetrics(&fm);
        return (fm.ascent + fm.descent + fm.lineGap) * this->size / fm.designUnitsPerEm;
    }

    return 0.0f;
}

bool ff::sprite_font::resource_load_complete(bool from_source)
{
    this->font_file = this->font_file_resource.object();
    return !from_source || this->init_sprites();
}

bool ff::sprite_font::init_sprites()
{
    IDWriteFontFaceX* font_face = this->font_file ? this->font_file->font_face() : nullptr;
    if (this->size <= 0.0f || this->size > 200.0f || !font_face)
    {
        return false;
    }

    DWRITE_FONT_METRICS1 font_metrics;
    font_face->GetMetrics(&font_metrics);
    float design_unit_size = this->size / font_metrics.designUnitsPerEm;

    struct sprite_info
    {
        size_t texture_index;
        ff::rect_float pos;
        ff::point_float handle;
    };

    std::vector<bool> has_glyph(ff::sprite_font::MAX_GLYPH_COUNT, false);
    std::vector<sprite_info> sprite_infos;
    sprite_infos.reserve(font_face->GetGlyphCount());

    // Map unicode characters to glyphs
    {
        uint32_t unicode_range_count;
        if (font_face->GetUnicodeRanges(0, nullptr, &unicode_range_count) != E_NOT_SUFFICIENT_BUFFER)
        {
            return false;
        }

        std::vector<DWRITE_UNICODE_RANGE> unicode_ranges;
        unicode_ranges.resize(unicode_range_count);
        if (FAILED(font_face->GetUnicodeRanges(unicode_range_count, unicode_ranges.data(), &unicode_range_count)))
        {
            return false;
        }

        for (const DWRITE_UNICODE_RANGE& ur : unicode_ranges)
        {
            for (uint32_t ch = ur.first; ch < ff::sprite_font::MAX_GLYPH_COUNT && ch <= ur.last; ch++)
            {
                uint16_t glyph;
                if (SUCCEEDED(font_face->GetGlyphIndices(&ch, 1, &glyph)))
                {
                    this->glyphs[ch].char_to_glyph = glyph;
                    has_glyph[glyph] = true;
                }
            }
        }
    }

    std::vector<DirectX::ScratchImage> staging_scratches;
    const ff::point_int staging_texture_size(1024, 1024);
    ff::point_int staging_pos(0, 0);
    int staging_row_height = 0;

    DirectX::ScratchImage staging_scratch;
    if (FAILED(staging_scratch.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, staging_texture_size.x, staging_texture_size.y, 1, 1)))
    {
        return false;
    }

    std::memset(staging_scratch.GetPixels(), 0, staging_scratch.GetPixelsSize());

    float glyph_advances = 0;
    std::vector<uint8_t> glyph_bytes;
    std::unordered_map<size_t, uint16_t, ff::no_hash<size_t>> hash_to_sprite;
    DWRITE_TEXTURE_TYPE glyph_texture_type = this->anti_alias ? DWRITE_TEXTURE_CLEARTYPE_3x1 : DWRITE_TEXTURE_ALIASED_1x1;

    const DWRITE_GLYPH_OFFSET zero_offset{};
    const DWRITE_MATRIX identity_transform{ 1.0, 0.0, 0.0, 1.0, 0.0, 0.0 };
    uint16_t glyph_id = 0;
    DWRITE_GLYPH_RUN gr{};
    gr.fontEmSize = this->size;
    gr.fontFace = font_face;
    gr.glyphAdvances = &glyph_advances;
    gr.glyphCount = 1;
    gr.glyphIndices = &glyph_id;
    gr.glyphOffsets = &zero_offset;

    for (size_t i = 0; i < ff::sprite_font::MAX_GLYPH_COUNT; i++)
    {
        if (!has_glyph[i])
        {
            continue;
        }

        glyph_id = static_cast<uint16_t>(i);

        DWRITE_GLYPH_METRICS gm{};
        if (FAILED(font_face->GetDesignGlyphMetrics(&glyph_id, 1, &gm)))
        {
            continue;
        }

        this->glyphs[i].glyph_width = gm.advanceWidth * design_unit_size;

        Microsoft::WRL::ComPtr<IDWriteGlyphRunAnalysis> gra;
        if (FAILED(ff::graphics::write_factory()->CreateGlyphRunAnalysis(
            &gr,
            &identity_transform,
            this->anti_alias ? DWRITE_RENDERING_MODE1_NATURAL : DWRITE_RENDERING_MODE1_ALIASED,
            DWRITE_MEASURING_MODE_NATURAL,
            DWRITE_GRID_FIT_MODE_DEFAULT,
            DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE,
            0, 0,
            &gra)))
        {
            continue;
        }

        RECT bounds;
        if (FAILED(gra->GetAlphaTextureBounds(glyph_texture_type, &bounds)))
        {
            continue;
        }

        ff::rect_int black_box(bounds.left, bounds.top, bounds.right, bounds.bottom);
        if (black_box.empty())
        {
            continue;
        }

        glyph_bytes.resize(black_box.area() * (this->anti_alias ? 3 : 1));
        if (FAILED(gra->CreateAlphaTexture(glyph_texture_type, &bounds, glyph_bytes.data(), static_cast<uint32_t>(glyph_bytes.size()))))
        {
            continue;
        }

        size_t glyph_bytes_hash = ff::stable_hash_bytes(glyph_bytes.data(), ff::vector_byte_size(glyph_bytes));
        auto iter = hash_to_sprite.find(glyph_bytes_hash);
        if (iter == hash_to_sprite.cend())
        {
            if (staging_pos.x + black_box.width() > staging_texture_size.x)
            {
                // Move down to the next row

                staging_pos.x = 0;
                staging_pos.y += staging_row_height + 1;
                staging_row_height = 0;
            }

            if (staging_pos.y + black_box.height() > staging_texture_size.y)
            {
                // Filled up this texture, make a new one

                staging_pos = ff::point_int{};
                staging_row_height = 0;

                staging_scratches.push_back(std::move(staging_scratch));
                if (FAILED(staging_scratch.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, staging_texture_size.x, staging_texture_size.y, 1, 1)))
                {
                    return false;
                }

                std::memset(staging_scratch.GetPixels(), 0, staging_scratch.GetPixelsSize());
            }

            // Copy bits to the texture

            size_t pixel_stride = this->anti_alias ? 3 : 1;
            for (int y = 0; y < black_box.height(); y++)
            {
                uint8_t* alpha_start = &glyph_bytes[y * black_box.width() * pixel_stride];
                uint8_t* data_start = staging_scratch.GetImages()->pixels + (staging_pos.y + y) * staging_scratch.GetImages()->rowPitch + staging_pos.x * 4;

                for (int x = 0; x < black_box.width(); x++)
                {
                    data_start[x * 4 + 0] = 255;
                    data_start[x * 4 + 1] = 255;
                    data_start[x * 4 + 2] = 255;

                    if (this->anti_alias)
                    {
                        float value =
                            static_cast<float>(alpha_start[x * pixel_stride + 0]) +
                            static_cast<float>(alpha_start[x * pixel_stride + 1]) +
                            static_cast<float>(alpha_start[x * pixel_stride + 2]);
                        data_start[x * 4 + 3] = static_cast<uint8_t>(static_cast<size_t>(value / 3) & 0xFF);
                    }
                    else
                    {
                        data_start[x * 4 + 3] = alpha_start[x * pixel_stride];
                    }
                }
            }

            // Add sprite to staging texture

            sprite_infos.push_back(sprite_info
                {
                    staging_scratches.size(),
                    ff::rect_int(staging_pos, staging_pos + black_box.size()).cast<float>(),
                    ff::point_float(-gm.leftSideBearing * design_unit_size, black_box.height() + gm.bottomSideBearing * design_unit_size),
                });

            staging_pos.x += black_box.width() + 1;
            staging_row_height = std::max(staging_row_height, black_box.height());

            iter = hash_to_sprite.try_emplace(glyph_bytes_hash, static_cast<uint16_t>(sprite_infos.size() - 1)).first;
        }

        this->glyphs[i].glyph_to_sprite = iter->second;
    }

    staging_scratches.push_back(std::move(staging_scratch));

    std::vector<std::shared_ptr<ff::dx11_texture_view_base>> textures;
    for (DirectX::ScratchImage& scratch : staging_scratches)
    {
        std::shared_ptr<ff::dx11_texture_view_base> view = std::make_shared<ff::dx11_texture>(std::make_shared<DirectX::ScratchImage>(std::move(scratch)));
        textures.push_back(std::move(view));
    }

    std::vector<ff::sprite> sprite_vector;
    std::vector<const ff::sprite_base*> sprite_pointers;
    sprite_vector.reserve(sprite_infos.size());
    sprite_pointers.reserve(sprite_infos.size());

    for (const sprite_info& info : sprite_infos)
    {
        sprite_vector.emplace_back("", textures[info.texture_index], info.pos, info.handle, ff::point_float(1, 1), ff::sprite_type::unknown);
    }

    for (const ff::sprite& sprite : sprite_vector)
    {
        sprite_pointers.push_back(&sprite);
    }

    this->sprites = std::make_shared<ff::sprite_list>(ff::internal::optimize_sprites(sprite_pointers, DXGI_FORMAT_BC2_UNORM, 1));
    if (this->sprites->size() != sprite_infos.size())
    {
        return false;
    }

    if (this->outline_thickness)
    {
        this->outline_sprites = std::make_shared<ff::sprite_list>(ff::internal::outline_sprites(sprite_pointers, DXGI_FORMAT_BC2_UNORM, 1));
        if (this->outline_sprites->size() != sprite_infos.size())
        {
            return false;
        }
    }

    return true;
}

ff::point_float ff::sprite_font::internal_draw_text(ff::draw_base* draw, const ff::sprite_list* sprites, std::wstring_view text, const ff::transform& transform, ff::sprite_font_options options) const
{
    IDWriteFontFaceX* font_face = this->font_file ? this->font_file->font_face() : nullptr;
    if (!font_face || text.empty() || transform.scale.x * transform.scale.y == 0.0f)
    {
        return {};
    }

    bool has_kerning = font_face->HasKerningPairs();
    DWRITE_FONT_METRICS1 fm{};
    font_face->GetMetrics(&fm);

    float design_unit_size = this->size / fm.designUnitsPerEm;
    ff::point_float scaled_design_unit_size = transform.scale * design_unit_size;
    ff::transform base_pos = transform;
    base_pos.position.y += fm.ascent * scaled_design_unit_size.y;
    ff::point_float max_pos(transform.position.x, transform.position.y + (fm.ascent + fm.descent) * scaled_design_unit_size.y);
    float line_spacing = (fm.ascent + fm.descent + fm.lineGap) * scaled_design_unit_size.y;

    if (draw)
    {
        draw->push_no_overlap();
    }

    for (const wchar_t* ch = text.data(), *ch_end = ch + text.size(); ch != ch_end; )
    {
        if (*ch == '\r' || *ch == '\n')
        {
            ch += (*ch == '\r' && ch + 1 != ch_end && ch[1] == '\n') ? 2 : 1;
            base_pos.position = ff::point_float(transform.position.x, base_pos.position.y + line_spacing);
            max_pos.y += line_spacing;
            continue;
        }
        else if (*ch >= static_cast<wchar_t>(ff::sprite_font_control::none) && *ch <= static_cast<wchar_t>(ff::sprite_font_control::after_last))
        {
            ff::sprite_font_control control = static_cast<ff::sprite_font_control>(*ch++);
            DirectX::XMFLOAT4 color;

            switch (control)
            {
                case ff::sprite_font_control::outline_color:
                case ff::sprite_font_control::text_color:
                    color.x = ((ch != ch_end) ? (int)*ch++ : 0) / 255.0f;
                    color.y = ((ch != ch_end) ? (int)*ch++ : 0) / 255.0f;
                    color.z = ((ch != ch_end) ? (int)*ch++ : 0) / 255.0f;
                    color.w = ((ch != ch_end) ? (int)*ch++ : 0) / 255.0f;

                    if (!ff::flags::has(options, ff::sprite_font_options::no_control))
                    {
                        if ((control == ff::sprite_font_control::outline_color && sprites == this->outline_sprites.get()) ||
                            (control == ff::sprite_font_control::text_color && sprites == this->sprites.get()))
                        {
                            base_pos.color = color;
                        }
                    }
                    break;

                case ff::sprite_font_control::outline_palette_color:
                case ff::sprite_font_control::text_palette_color:
                    ff::palette_index_to_color((ch != ch_end) ? static_cast<int>(*ch++) : 0, color);

                    if (!ff::flags::has(options, ff::sprite_font_options::no_control))
                    {
                        if ((control == ff::sprite_font_control::outline_palette_color && sprites == this->outline_sprites.get()) ||
                            (control == ff::sprite_font_control::text_palette_color && sprites == this->sprites.get()))
                        {
                            base_pos.color = color;
                        }
                    }
                    break;
            }
        }
        else
        {
            const ff::sprite_font::char_and_glyph_info& glyph = this->glyphs[this->glyphs[*ch].char_to_glyph];

            if (draw && sprites && glyph.glyph_to_sprite && glyph.glyph_to_sprite < sprites->size())
            {
                draw->draw_sprite(sprites->get(static_cast<size_t>(glyph.glyph_to_sprite))->sprite_data(), base_pos);
            }

            base_pos.position.x += glyph.glyph_width * base_pos.scale.x;
            max_pos.x = std::max(max_pos.x, base_pos.position.x);

            if (has_kerning && ch + 1 != ch_end)
            {
                uint16_t two_glyphs[2] = { ch[0], ch[1] };
                int two_design_kerns[2];
                if (SUCCEEDED(font_face->GetKerningPairAdjustments(2, two_glyphs, two_design_kerns)))
                {
                    base_pos.position.x += two_design_kerns[0] * scaled_design_unit_size.x;
                }
            }

            ch++;
        }
    }

    if (draw)
    {
        draw->pop_no_overlap();
    }

    return max_pos - transform.position;
}

std::vector<std::shared_ptr<ff::resource>> ff::sprite_font::resource_get_dependencies() const
{
    return std::vector<std::shared_ptr<resource>>
    {
        this->font_file_resource.resource()
    };
}

bool ff::sprite_font::save_to_cache(ff::dict& dict, bool& allow_compress) const
{
    ff::dict sprites_dict, outline_sprites_dict;
    if (*this && ff::resource_object_base::save_to_cache_typed(*this->sprites, sprites_dict, allow_compress) &&
        (!this->outline_sprites || ff::resource_object_base::save_to_cache_typed(*this->outline_sprites, outline_sprites_dict, allow_compress)))
    {
        dict.set<ff::resource>("data", this->font_file_resource.resource());
        dict.set<float>("size", this->size);
        dict.set<int>("outline", this->outline_thickness);
        dict.set<bool>("aa", this->anti_alias);
        dict.set_bytes("glyphs", this->glyphs.data(), ff::array_byte_size(this->glyphs));
        dict.set<ff::dict>("sprites", std::move(sprites_dict));
        dict.set<ff::dict>("outline_sprites", std::move(outline_sprites_dict));

        return true;
    }

    return false;
}

std::shared_ptr<ff::resource_object_base> ff::internal::sprite_font_factory::load_from_source(const ff::dict& dict, resource_load_context& context) const
{
    std::shared_ptr<ff::resource> font_file_resource = dict.get<ff::resource>("data");
    float size = dict.get<float>("size");
    int outline_thickness = dict.get<int>("outline");
    bool anti_alias = dict.get<bool>("aa");

    if (font_file_resource)
    {
        return std::make_shared<ff::sprite_font>(font_file_resource, size, outline_thickness, anti_alias);
    }

    assert(false);
    return nullptr;
}

std::shared_ptr<ff::resource_object_base> ff::internal::sprite_font_factory::load_from_cache(const ff::dict& dict) const
{
    std::shared_ptr<ff::resource> font_file_resource = dict.get<ff::resource>("data");
    float size = dict.get<float>("size");
    int outline_thickness = dict.get<int>("outline");
    bool anti_alias = dict.get<bool>("aa");
    std::shared_ptr<ff::data_base> glyphs_data = dict.get<ff::data_base>("glyphs");
    std::shared_ptr<ff::sprite_list> sprites = std::dynamic_pointer_cast<ff::sprite_list>(dict.get<ff::resource_object_base>("sprites"));
    std::shared_ptr<ff::sprite_list> outline_sprites = std::dynamic_pointer_cast<ff::sprite_list>(dict.get<ff::resource_object_base>("outline_sprites"));

    if (font_file_resource && glyphs_data && sprites)
    {
        return std::make_shared<ff::sprite_font>(font_file_resource, size, outline_thickness, anti_alias, sprites, outline_sprites, glyphs_data);
    }

    assert(false);
    return nullptr;
}
