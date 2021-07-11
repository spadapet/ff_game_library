#include "pch.h"
#include "dxgi_util.h"
#include "palette_data.h"
#include "sprite_base.h"
#include "sprite_optimizer.h"
#include "texture.h"
#include "texture_util.h"

static const int TEXTURE_SIZE_MAX = 1024;
static const int TEXTURE_SIZE_MIN = 128;
static const int TEXTURE_GRID_SIZE = 8;

namespace
{
    // Info about where each sprite came from and where it's going
    struct optimized_sprite_info
    {
        optimized_sprite_info(const ff::sprite_base* sprite, size_t sprite_index)
            : sprite(sprite)
            , dest_sprite_type(ff::sprite_type::unknown)
            , source_rect(sprite->sprite_data().texture_rect().offset(0.5f, 0.5f).cast<int>())
            , dest_rect{}
            , sprite_index(sprite_index)
            , dest_texture(ff::constants::invalid_size)
        {}

        optimized_sprite_info(optimized_sprite_info&& other) noexcept = default;
        optimized_sprite_info(const optimized_sprite_info& other) = default;

        optimized_sprite_info& operator=(optimized_sprite_info&& other) noexcept = default;
        optimized_sprite_info& operator=(const optimized_sprite_info& other) = default;

        bool operator<(const optimized_sprite_info& other) const
        {
            if (this->source_rect.height() == other.source_rect.height())
            {
                if (this->source_rect.width() == other.source_rect.width())
                {
                    if (this->source_rect.top == other.source_rect.top)
                    {
                        if (this->source_rect.left == other.source_rect.left)
                        {
                            return this->sprite->sprite_data().view() < other.sprite->sprite_data().view();
                        }

                        return this->source_rect.left < other.source_rect.left;
                    }

                    return this->source_rect.top < other.source_rect.top;
                }

                // larger comes first
                return this->source_rect.width() > other.source_rect.width();
            }

            // larger comes first
            return this->source_rect.height() > other.source_rect.height();
        }

        const ff::sprite_base* sprite;
        ff::sprite_type dest_sprite_type;
        ff::rect_int source_rect;
        ff::rect_int dest_rect;
        size_t sprite_index;
        size_t dest_texture;
    };

    // Cached RGBA original texture
    struct original_texture_info
    {
        std::shared_ptr<ff::texture> rgb_texture;
        std::shared_ptr<DirectX::ScratchImage> rgb_scratch;
    };

    // Destination texture RGBA (_texture) and final converted texture (_finalTexture)
    struct optimized_texture_info
    {
        optimized_texture_info(ff::point_int size)
            : size(size)
            , row_left{}
            , row_right{}
        {
            // Column indexes must fit within a byte (even one beyond the last column)
            assert(::TEXTURE_SIZE_MAX / ::TEXTURE_GRID_SIZE < 256 && this->row_left.size() == this->row_right.size());
        }

        optimized_texture_info(const optimized_texture_info& other) = default;
        optimized_texture_info(optimized_texture_info&& other) noexcept = default;

        optimized_texture_info& operator=(const optimized_texture_info& other) = default;
        optimized_texture_info& operator=(optimized_texture_info&& other) noexcept = default;

        ff::rect_int find_placement(ff::point_int placement_size)
        {
            ff::point_int dest_pos(-1, -1);

            if (placement_size.x > 0 && placement_size.x <= this->size.x && placement_size.y > 0 && placement_size.y <= this->size.y)
            {
                ff::point_int cell_size(
                    (placement_size.x + ::TEXTURE_GRID_SIZE - 1) / ::TEXTURE_GRID_SIZE,
                    (placement_size.y + ::TEXTURE_GRID_SIZE - 1) / ::TEXTURE_GRID_SIZE);

                for (int y = 0; y + cell_size.y <= this->size.y / ::TEXTURE_GRID_SIZE; y++)
                {
                    for (int attempt = 0; attempt < 2; attempt++)
                    {
                        // Try to put the sprite as far left as possible in each row
                        int x;

                        if (attempt)
                        {
                            x = this->row_right[y];

#if FORCE_GAP
                            if (x)
                            {
                                // don't touch the previous sprite
                                x++;
                            }
#endif
                        }
                        else
                        {
                            x = this->row_left[y];
#if FORCE_GAP
                            x -= cell_size.x + 1;
#else
                            x -= cell_size.x;
#endif
                        }

                        if (x >= 0 && x + cell_size.x <= this->size.x / ::TEXTURE_GRID_SIZE)
                        {
                            bool found = true;

#if FORCE_GAP
                            // Look for intersection with another sprite
                            for (int check_y = y + cell_size.y; check_y >= y - 1; check_y--)
                            {
                                if (check_y >= 0 &&
                                    check_y < this->size.y / TEXTURE_GRID_SIZE &&
                                    check_y < _countof(this->row_right) &&
                                    this->row_right[check_y] &&
                                    this->row_right[check_y] + 1 > x &&
                                    x + cell_size.x + 1 > this->row_left[check_y])
                                {
                                    found = false;
                                    break;
                                }
                            }
#else
                            // Look for intersection with another sprite
                            for (int check_y = y + cell_size.y - 1; check_y >= y; check_y--)
                            {
                                if (check_y >= 0 &&
                                    check_y < this->size.y / ::TEXTURE_GRID_SIZE &&
                                    static_cast<size_t>(check_y) < this->row_right.size() &&
                                    this->row_right[check_y] &&
                                    this->row_right[check_y] > x &&
                                    x + cell_size.x > this->row_left[check_y])
                                {
                                    found = false;
                                    break;
                                }
                            }
#endif

                            // Prefer positions further to the left
                            if (found && (dest_pos.x == -1 || dest_pos.x > x * ::TEXTURE_GRID_SIZE))
                            {
                                dest_pos = ff::point_int(x * ::TEXTURE_GRID_SIZE, y * ::TEXTURE_GRID_SIZE);
                            }
                        }
                    }
                }
            }

            return (dest_pos.x == -1)
                ? ff::rect_int(0, 0, 0, 0)
                : ff::rect_int(dest_pos.x, dest_pos.y, dest_pos.x + placement_size.x, dest_pos.y + placement_size.y);
        }

        bool place_rect(ff::rect_int rect)
        {
            ff::rect_int rect_cells(
                rect.left / ::TEXTURE_GRID_SIZE,
                rect.top / ::TEXTURE_GRID_SIZE,
                (rect.right + ::TEXTURE_GRID_SIZE - 1) / ::TEXTURE_GRID_SIZE,
                (rect.bottom + ::TEXTURE_GRID_SIZE - 1) / ::TEXTURE_GRID_SIZE);

            if (!(rect.left >= 0 && rect.right <= this->size.x &&
                rect.top >= 0 && rect.bottom <= this->size.y &&
                rect.width() > 0 &&
                rect.height() > 0))
            {
                assert(false);
                return false;
            }

            // Validate that the sprite doesn't overlap anything

#if FORCE_GAP
            for (int y = rect_cells.top - 1; y <= rect_cells.bottom; y++)
            {
                if (y >= 0 && y < this->size.y / ::TEXTURE_GRID_SIZE && y < this->row_right.size())
                {
                    // Must be a one cell gap between sprites
                    if (this->row_right[y] &&
                        this->row_right[y] + 1 > rect_cells.left &&
                        rect_cells.right + 1 > this->row_left[y])
                    {
                        assert(false);
                        return false;
                    }
                }
            }
#else
            for (int y = rect_cells.top; y < rect_cells.bottom; y++)
            {
                if (y >= 0 && y < this->size.y / TEXTURE_GRID_SIZE && static_cast<size_t>(y) < this->row_right.size())
                {
                    // Must be a one cell gap between sprites
                    if (this->row_right[y] &&
                        this->row_right[y] > rect_cells.left &&
                        rect_cells.right > this->row_left[y])
                    {
                        assert(false);
                        return false;
                    }
                }
            }
#endif
            // Invalidate the space taken up by the new sprite

            for (int y = rect_cells.top; y < rect_cells.bottom; y++)
            {
                if (y >= 0 && y < this->size.y / TEXTURE_GRID_SIZE && static_cast<size_t>(y) < this->row_right.size())
                {
                    if (this->row_right[y])
                    {
                        this->row_left[y] = std::min<BYTE>(this->row_left[y], rect_cells.left);
                        this->row_right[y] = std::max<BYTE>(this->row_right[y], rect_cells.right);
                    }
                    else
                    {
                        this->row_left[y] = rect_cells.left;
                        this->row_right[y] = rect_cells.right;
                    }
                }
            }

            return true;
        }

        ff::point_int size;
        DirectX::ScratchImage scratch_texture;
        std::shared_ptr<ff::texture> final_texture;

    private:
        std::array<uint8_t, ::TEXTURE_SIZE_MAX / ::TEXTURE_GRID_SIZE> row_left;
        std::array<uint8_t, ::TEXTURE_SIZE_MAX / ::TEXTURE_GRID_SIZE> row_right;
    };
}

// Returns true when all done (sprites will still be placed when false is returned)
static bool place_sprites(std::vector<::optimized_sprite_info>& sprites, std::vector<optimized_texture_info>& texture_infos, size_t start_texture)
{
    size_t sprites_done = 0;

    for (size_t i = 0; i < sprites.size(); i++)
    {
        ::optimized_sprite_info& sprite = sprites[i];
        bool reuse_previous = (i > 0) && !(sprite < sprites[i - 1]) && !(sprites[i - 1] < sprite);

        if (reuse_previous)
        {
            sprite.dest_texture = sprites[i - 1].dest_texture;
            sprite.dest_rect = sprites[i - 1].dest_rect;
        }
        else
        {
            if (sprite.dest_texture == ff::constants::invalid_size)
            {
                if (sprite.source_rect.width() > ::TEXTURE_SIZE_MAX ||
                    sprite.source_rect.height() > ::TEXTURE_SIZE_MAX)
                {
                    ff::point_int size = sprite.source_rect.size();

                    // Oversized, so make a new unshared texture
                    sprite.dest_texture = texture_infos.size();
                    sprite.dest_rect = ff::rect_int(ff::point_int(0, 0), size);

                    // texture sizes should be powers of 2 to support compression and mipmaps
                    size.x = ff::math::nearest_power_of_two(size.x);
                    size.y = ff::math::nearest_power_of_two(size.y);

                    texture_infos.emplace_back(size);
                }
            }

            if (sprite.dest_texture == ff::constants::invalid_size)
            {
                // Look for empty space in an existing texture
                for (size_t h = start_texture; h < texture_infos.size(); h++)
                {
                    ::optimized_texture_info& texture = texture_infos[h];

                    if (texture.size.x <= ::TEXTURE_SIZE_MAX &&
                        texture.size.y <= ::TEXTURE_SIZE_MAX)
                    {
                        sprite.dest_rect = texture.find_placement(sprite.source_rect.size());

                        if (sprite.dest_rect != ff::rect_int{})
                        {
                            bool success = texture.place_rect(sprite.dest_rect);
                            assert(success);
                            sprite.dest_texture = h;
                            break;
                        }
                    }
                }
            }
        }

        if (sprite.dest_texture != ff::constants::invalid_size)
        {
            sprites_done++;
        }
    }

    return sprites_done == sprites.size();
}

static std::vector<::optimized_sprite_info> create_sprite_infos(const std::vector<const ff::sprite_base*>& original_sprites)
{
    std::vector<::optimized_sprite_info> sprite_infos;
    sprite_infos.reserve(original_sprites.size());

    for (size_t i = 0; i < original_sprites.size(); i++)
    {
        sprite_infos.emplace_back(original_sprites[i], i);
    }

    return sprite_infos;
}

static bool create_original_textures(
    DXGI_FORMAT format,
    const std::vector<::optimized_sprite_info>& sprite_infos,
    std::unordered_map<const ff::texture*, ::original_texture_info>& original_textures,
    std::shared_ptr<DirectX::ScratchImage>& palette_data)
{
    for (const ::optimized_sprite_info& sprite_info : sprite_infos)
    {
        const ff::texture* texture = sprite_info.sprite->sprite_data().view()->view_texture();
        if (!palette_data)
        {
            palette_data = texture->palette();
        }

        if (original_textures.find(texture) == original_textures.cend())
        {
            if (texture->format() != format && (!ff::internal::color_format(texture->format()) || !ff::internal::color_format(format)))
            {
                assert(false);
                return false;
            }

            DXGI_FORMAT capture_format = ff::internal::color_format(format) ? ff::internal::DEFAULT_FORMAT : format;
            ::original_texture_info texure_info;
            texure_info.rgb_texture = std::make_shared<ff::texture>(*texture, capture_format, 1);
            if (!texure_info.rgb_texture)
            {
                assert(false);
                return false;
            }

            texure_info.rgb_scratch = texure_info.rgb_texture->data();
            if (!texure_info.rgb_scratch)
            {
                assert(false);
                return false;
            }

            original_textures.try_emplace(std::move(texture), std::move(texure_info));
        }
    }

    return true;
}

static bool compute_optimized_sprites(std::vector<::optimized_sprite_info>& sprites, std::vector<::optimized_texture_info>& texture_infos)
{
    for (bool done = false; !done && sprites.size(); )
    {
        // Add a new texture, start with the smallest size and work up
        for (ff::point_int size(::TEXTURE_SIZE_MIN, ::TEXTURE_SIZE_MIN); !done && size.x <= ::TEXTURE_SIZE_MAX; size *= 2)
        {
            size_t texture_info = texture_infos.size();
            texture_infos.emplace_back(size);

            done = ::place_sprites(sprites, texture_infos, texture_infos.size() - 1);

            if (!done && size.x < ::TEXTURE_SIZE_MAX)
            {
                // Remove this texture and use a bigger one instead
                texture_infos.pop_back();

                for (::optimized_sprite_info& sprite : sprites)
                {
                    if (sprite.dest_texture != ff::constants::invalid_size)
                    {
                        if (sprite.dest_texture == texture_info)
                        {
                            sprite.dest_texture = ff::constants::invalid_size;
                            sprite.dest_rect = ff::rect_int{};
                        }
                        else if (sprite.dest_texture > texture_info)
                        {
                            sprite.dest_texture--;
                        }
                    }
                }
            }
        }
    }

    return true;
}

static bool create_optimized_textures(DXGI_FORMAT format, std::vector<::optimized_texture_info>& texture_infos)
{
    format = ff::internal::color_format(format) ? ff::internal::DEFAULT_FORMAT : format;

    for (::optimized_texture_info& texture : texture_infos)
    {
        if (FAILED(texture.scratch_texture.Initialize2D(format, texture.size.x, texture.size.y, 1, 1)))
        {
            assert(false);
            return false;
        }

        std::memset(texture.scratch_texture.GetPixels(), 0, texture.scratch_texture.GetPixelsSize());
    }

    return true;
}

static bool copy_optimized_sprites(
    std::vector<::optimized_sprite_info>& sprite_infos,
    std::unordered_map<const ff::texture*, ::original_texture_info>& original_textures,
    std::vector<::optimized_texture_info>& texture_infos)
{
    for (::optimized_sprite_info& sprite : sprite_infos)
    {
        auto iter = original_textures.find(sprite.sprite->sprite_data().view()->view_texture());
        if (sprite.dest_texture >= texture_infos.size() || iter == original_textures.cend())
        {
            assert(false);
            return false;
        }

        ::original_texture_info& original_info = iter->second;
        sprite.dest_sprite_type = ff::internal::get_sprite_type(*original_info.rgb_scratch, &sprite.source_rect.cast<size_t>());

        bool status = SUCCEEDED(DirectX::CopyRectangle(
            *original_info.rgb_scratch->GetImages(),
            DirectX::Rect(
                sprite.source_rect.left,
                sprite.source_rect.top,
                sprite.source_rect.width(),
                sprite.source_rect.height()),
            *texture_infos[sprite.dest_texture].scratch_texture.GetImages(),
            DirectX::TEX_FILTER_DEFAULT,
            sprite.dest_rect.left,
            sprite.dest_rect.top));
        assert(status);
    }

    return true;
}

static bool convert_final_textures(
    DXGI_FORMAT format,
    size_t mip_count,
    std::vector<::optimized_texture_info>& texture_infos,
    const std::shared_ptr<DirectX::ScratchImage>& palette_scratch)
{
    for (::optimized_texture_info& texure_info : texture_infos)
    {
        std::shared_ptr<ff::texture> rgb_texture = std::make_shared<ff::texture>(
            std::make_shared<DirectX::ScratchImage>(std::move(texure_info.scratch_texture)), palette_scratch);
        if (!*rgb_texture)
        {
            assert(false);
            return false;
        }

        texure_info.final_texture = std::make_shared<ff::texture>(*rgb_texture, format, mip_count);
        if (!*texure_info.final_texture)
        {
            assert(false);
            return false;
        }
    }

    return true;
}

static bool create_final_sprites(
    const std::vector<::optimized_sprite_info>& sprite_infos,
    std::vector<::optimized_texture_info>& texture_infos,
    std::vector<ff::sprite>& new_sprites)
{
    new_sprites.reserve(sprite_infos.size());

    for (const ::optimized_sprite_info& sprite_info : sprite_infos)
    {
        new_sprites.emplace_back(
            std::string(sprite_info.sprite->name()),
            texture_infos[sprite_info.dest_texture].final_texture,
            sprite_info.dest_rect.cast<float>(),
            sprite_info.sprite->sprite_data().handle(),
            sprite_info.sprite->sprite_data().scale(),
            sprite_info.dest_sprite_type);
    }

    return true;
}

std::vector<ff::sprite> ff::internal::optimize_sprites(const std::vector<const ff::sprite_base*>& old_sprites, DXGI_FORMAT new_format, size_t new_mip_count)
{
    std::vector<ff::sprite> new_sprites;

    if (new_mip_count != 1 && !ff::internal::color_format(new_format))
    {
        assert(false);
        return new_sprites;
    }

    std::vector<::optimized_sprite_info> sprite_infos = ::create_sprite_infos(old_sprites);
    std::sort(sprite_infos.begin(), sprite_infos.end());

    std::unordered_map<const ff::texture*, ::original_texture_info> original_textures;
    std::shared_ptr<DirectX::ScratchImage> scratch_palette;
    std::vector<::optimized_texture_info> texture_infos;

    if (!::create_original_textures(new_format, sprite_infos, original_textures, scratch_palette) ||
        !::compute_optimized_sprites(sprite_infos, texture_infos) ||
        !::create_optimized_textures(new_format, texture_infos))
    {
        assert(false);
        return new_sprites;
    }

    // Go back to the original order
    std::sort(sprite_infos.begin(), sprite_infos.end(), [](const ::optimized_sprite_info& info1, const ::optimized_sprite_info& info2)
        {
            return info1.sprite_index < info2.sprite_index;
        });

    if (!::copy_optimized_sprites(sprite_infos, original_textures, texture_infos) ||
        !::convert_final_textures(new_format, new_mip_count, texture_infos, scratch_palette) ||
        !::create_final_sprites(sprite_infos, texture_infos, new_sprites))
    {
        assert(false);
    }

    return new_sprites;
}

static bool create_outline_sprites(
    DXGI_FORMAT format,
    std::vector<::optimized_sprite_info>& sprite_infos,
    const std::unordered_map<const ff::texture*, ::original_texture_info>& original_textures,
    std::vector<std::shared_ptr<ff::texture>>& outline_textures,
    std::vector<ff::sprite>& outline_sprite_list,
    const std::shared_ptr<DirectX::ScratchImage>& palette_data)
{
    bool use_palette = ff::internal::palette_format(format);
    const int pixel_size = use_palette ? 1 : 4;
    const int alpha_offset = use_palette ? 0 : 3;

    for (::optimized_sprite_info& sprite_info : sprite_infos)
    {
        const ff::sprite_data& sprite_data = sprite_info.sprite->sprite_data();
        const ff::texture* original_texture = sprite_data.view()->view_texture();
        auto iter = original_textures.find(original_texture);
        if (iter == original_textures.cend())
        {
            assert(false);
            return false;
        }

        const ::original_texture_info& texture_info = iter->second;
        const ff::rect_int& src_rect = sprite_info.source_rect;

        DirectX::ScratchImage outline_scratch;
        if (FAILED(outline_scratch.Initialize2D(
            use_palette ? DXGI_FORMAT_R8_UINT : DXGI_FORMAT_R8G8B8A8_UNORM,
            static_cast<size_t>(src_rect.width()) + 2,
            static_cast<size_t>(src_rect.height()) + 2,
            1, 1)))
        {
            assert(false);
            return false;
        }

        std::memset(outline_scratch.GetPixels(), 0, outline_scratch.GetPixelsSize());

        const DirectX::Image& src_image = *texture_info.rgb_scratch->GetImages();
        const DirectX::Image& dest_image = *outline_scratch.GetImages();

        for (ff::point_int xy(src_rect.left, src_rect.top), dest_xy = ff::point_int{}; xy.y < src_rect.bottom; xy.y++, dest_xy.y++)
        {
            const uint8_t* src_row = src_image.pixels + src_image.rowPitch * xy.y + src_rect.left * pixel_size;

            for (xy.x = src_rect.left, dest_xy.x = 0; xy.x < src_rect.right; xy.x++, dest_xy.x++, src_row += pixel_size)
            {
                if (src_row[alpha_offset]) // alpha is set
                {
                    uint8_t* dest = dest_image.pixels + dest_image.rowPitch * dest_xy.y + dest_xy.x * pixel_size;

                    if (use_palette)
                    {
                        dest[0] = 1;
                        dest[1] = 1;
                        dest[2] = 1;

                        dest += dest_image.rowPitch;
                        dest[0] = 1;
                        dest[1] = 1;
                        dest[2] = 1;

                        dest += dest_image.rowPitch;
                        dest[0] = 1;
                        dest[1] = 1;
                        dest[2] = 1;
                    }
                    else
                    {
                        reinterpret_cast<uint32_t*>(dest)[0] = 0xFFFFFFFF;
                        reinterpret_cast<uint32_t*>(dest)[1] = 0xFFFFFFFF;
                        reinterpret_cast<uint32_t*>(dest)[2] = 0xFFFFFFFF;

                        dest += dest_image.rowPitch;
                        reinterpret_cast<uint32_t*>(dest)[0] = 0xFFFFFFFF;
                        reinterpret_cast<uint32_t*>(dest)[1] = 0xFFFFFFFF;
                        reinterpret_cast<uint32_t*>(dest)[2] = 0xFFFFFFFF;

                        dest += dest_image.rowPitch;
                        reinterpret_cast<uint32_t*>(dest)[0] = 0xFFFFFFFF;
                        reinterpret_cast<uint32_t*>(dest)[1] = 0xFFFFFFFF;
                        reinterpret_cast<uint32_t*>(dest)[2] = 0xFFFFFFFF;
                    }
                }
            }
        }

        auto outline_texture = std::make_shared<ff::texture>(std::make_shared<DirectX::ScratchImage>(std::move(outline_scratch)), palette_data);
        outline_textures.push_back(outline_texture);

        outline_sprite_list.emplace_back(
            std::string(sprite_info.sprite->name()),
            std::shared_ptr<ff::texture_view_base>(outline_texture),
            ff::rect_float(ff::point_float{}, outline_texture->size().cast<float>()),
            sprite_data.handle() + ff::point_float(1, 1),
            sprite_data.scale(),
            ff::sprite_type::unknown);
    }

    return true;
}

std::vector<ff::sprite> ff::internal::outline_sprites(const std::vector<const ff::sprite_base*>& old_sprites, DXGI_FORMAT new_format, size_t new_mip_count)
{
    std::vector<ff::sprite> new_sprites;

    if (!ff::internal::color_format(new_format) && !ff::internal::palette_format(new_format))
    {
        assert(false);
        return new_sprites;
    }

    std::vector<::optimized_sprite_info> sprite_infos = ::create_sprite_infos(old_sprites);
    std::vector<std::shared_ptr<ff::texture>> outline_textures;
    std::unordered_map<const ff::texture*, ::original_texture_info> original_textures;
    std::shared_ptr<DirectX::ScratchImage> palette_data;

    if (!::create_original_textures(new_format, sprite_infos, original_textures, palette_data) ||
        !::create_outline_sprites(new_format, sprite_infos, original_textures, outline_textures, new_sprites, palette_data))
    {
        assert(false);
        return new_sprites;
    }

    std::vector<const ff::sprite_base*> new_sprite_pointers;
    for (auto& sprite : new_sprites)
    {
        new_sprite_pointers.push_back(&sprite);
    }

    return ff::internal::optimize_sprites(new_sprite_pointers, new_format, new_mip_count);
}
