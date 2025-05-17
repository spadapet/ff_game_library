#include "pch.h"
#include "graphics/dxgi/format_util.h"
#include "graphics/dxgi/dxgi_globals.h"
#include "graphics/dxgi/sprite_data.h"
#include "graphics/resource/palette_data.h"
#include "graphics/resource/sprite_optimizer.h"
#include "graphics/resource/texture_data.h"
#include "graphics/resource/texture_resource.h"

#pragma warning(disable : 4267)
#include "../vendor/RectangleBinPack/MaxRectsBinPack.cpp"
#include "../vendor/RectangleBinPack/Rect.cpp"
#pragma warning(default : 4267)

constexpr int TEXTURE_SIZE_MAX = 1024;
constexpr int TEXTURE_SIZE_MIN = 128;
constexpr int BORDER_SIZE = 2;

namespace
{
    // Info about where each sprite came from and where it's going
    struct optimized_sprite_info
    {
        optimized_sprite_info(const ff::sprite* sprite, size_t sprite_index)
            : sprite(sprite)
            , sprite_index(sprite_index)
            , dest_sprite_type(ff::dxgi::sprite_type::unknown)
            , dest_texture(ff::constants::invalid_unsigned<size_t>())
            , source_rect(sprite->sprite_data().texture_rect().offset(0.5f, 0.5f).cast<int>())
            , dest_rect{}
        {
        }

        bool has_dest_texture() const
        {
            return this->dest_texture != ff::constants::invalid_unsigned<size_t>();
        }

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
                            // Keep the same views together to detect dupe sprites
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

        const ff::sprite* sprite;
        ff::dxgi::sprite_type dest_sprite_type;
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
            , packer(std::make_unique<rbp::MaxRectsBinPack>(size.x, size.y, false))
        {
        }

        bool find_placement(ff::point_int placement_size, ff::rect_int& dest_rect)
        {
            rbp::Rect dest = this->packer->Insert(placement_size.x, placement_size.y, rbp::MaxRectsBinPack::FreeRectChoiceHeuristic::RectBestAreaFit);
            dest_rect = ff::rect_int(dest.x, dest.y, dest.x + dest.width, dest.y + dest.height);
            return dest.width && dest.height;
        }

        ff::point_int size;
        DirectX::ScratchImage scratch_texture;
        std::shared_ptr<ff::texture> final_texture;

    private:
        std::unique_ptr<rbp::MaxRectsBinPack> packer;
    };
}

// Returns true when all done (sprites will still be placed when false is returned)
static bool place_sprites(std::vector<::optimized_sprite_info>& sprites, std::vector<optimized_texture_info>& texture_infos, size_t start_texture)
{
    size_t sprites_done = 0;

    for (size_t i = 0; i < sprites.size(); i++)
    {
        ::optimized_sprite_info& sprite = sprites[i];
        ff::point_int size = sprite.source_rect.size() + ff::point_int{::BORDER_SIZE * 2, ::BORDER_SIZE * 2};
        bool oversized = size.x > ::TEXTURE_SIZE_MAX || size.y > ::TEXTURE_SIZE_MAX;

        if (sprite.has_dest_texture())
        {
            // Already placed
        }
        else if (i > 0 && !(sprite < sprites[i - 1]) && !(sprites[i - 1] < sprite))
        {
            // The previous sprite is exactly the same
            sprite.dest_texture = sprites[i - 1].dest_texture;
            sprite.dest_rect = sprites[i - 1].dest_rect;
        }
        else if (oversized)
        {
            // No need for borders
            size = sprite.source_rect.size();

            // Oversized, so make a new unshared texture
            sprite.dest_texture = texture_infos.size();
            sprite.dest_rect = ff::rect_int(ff::point_int(0, 0), size);

            // texture sizes should be powers of 2 to support compression and mipmaps
            size.x = ff::math::nearest_power_of_two(size.x);
            size.y = ff::math::nearest_power_of_two(size.y);

            texture_infos.emplace_back(size);
        }
        else
        {
            // Look for empty space in an existing texture
            for (size_t h = start_texture; h < texture_infos.size() && !sprite.has_dest_texture(); h++)
            {
                if (texture_infos[h].find_placement(size, sprite.dest_rect))
                {
                    sprite.dest_texture = h;
                }
            }
        }

        if (sprite.has_dest_texture())
        {
            sprites_done++;
        }
    }

    return sprites_done == sprites.size();
}

static bool compute_optimized_sprites(std::vector<::optimized_sprite_info>& sprites, std::vector<::optimized_texture_info>& texture_infos)
{
    for (bool done = false; !done && sprites.size(); )
    {
        // Add a new texture, start with the smallest size and work up
        for (ff::point_int size(::TEXTURE_SIZE_MIN, ::TEXTURE_SIZE_MIN); !done && size.x <= ::TEXTURE_SIZE_MAX; size *= 2)
        {
            size_t texture_index = texture_infos.size();
            texture_infos.emplace_back(size);

            done = ::place_sprites(sprites, texture_infos, texture_infos.size() - 1);

            if (!done && size.x < ::TEXTURE_SIZE_MAX)
            {
                // Remove this texture and use a bigger one instead
                texture_infos.erase(texture_infos.begin() + texture_index);

                for (::optimized_sprite_info& sprite : sprites)
                {
                    if (sprite.has_dest_texture())
                    {
                        if (sprite.dest_texture == texture_index)
                        {
                            sprite.dest_texture = ff::constants::invalid_unsigned<size_t>();
                            sprite.dest_rect = {};
                        }
                        else if (sprite.dest_texture > texture_index)
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

static std::vector<::optimized_sprite_info> create_sprite_infos(const std::vector<ff::sprite>& original_sprites)
{
    std::vector<::optimized_sprite_info> sprite_infos;
    sprite_infos.reserve(original_sprites.size());

    for (size_t i = 0; i < original_sprites.size(); i++)
    {
        sprite_infos.emplace_back(&original_sprites[i], i);
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
        const ff::texture* texture = sprite_info.sprite->texture().get();
        if (!palette_data)
        {
            palette_data = texture->palette();
        }

        if (original_textures.find(texture) == original_textures.cend())
        {
            if (texture->dxgi_texture()->format() != format && (!ff::dxgi::color_format(texture->dxgi_texture()->format()) || !ff::dxgi::color_format(format)))
            {
                debug_fail_ret_val(false);
            }

            DXGI_FORMAT capture_format = ff::dxgi::color_format(format) ? DXGI_FORMAT_R8G8B8A8_UNORM : format;
            ::original_texture_info texure_info;
            texure_info.rgb_texture = std::make_shared<ff::texture>(*texture, capture_format, 1);
            assert_ret_val(texure_info.rgb_texture, false);

            texure_info.rgb_scratch = texure_info.rgb_texture->dxgi_texture()->data();
            assert_ret_val(texure_info.rgb_scratch, false);

            original_textures.try_emplace(std::move(texture), std::move(texure_info));
        }
    }

    return true;
}

static bool create_optimized_textures(DXGI_FORMAT format, std::vector<::optimized_texture_info>& texture_infos)
{
    format = ff::dxgi::color_format(format) ? DXGI_FORMAT_R8G8B8A8_UNORM : format;

    for (::optimized_texture_info& texture : texture_infos)
    {
        assert_hr_ret_val(texture.scratch_texture.Initialize2D(format, texture.size.x, texture.size.y, 1, 1), false);
        std::memset(texture.scratch_texture.GetPixels(), 0, texture.scratch_texture.GetPixelsSize());
    }

    return true;
}

static bool copy_optimized_sprites(
    std::vector<::optimized_sprite_info>& sprite_infos,
    std::unordered_map<const ff::texture*, ::original_texture_info>& original_textures,
    std::vector<::optimized_texture_info>& texture_infos)
{
    std::vector<ff::co_task_source<bool>> tasks;
    tasks.reserve(sprite_infos.size());

    for (::optimized_sprite_info& sprite : sprite_infos)
    {
        tasks.emplace_back(ff::task::run<bool>([&sprite, &original_textures, &texture_infos]()
        {
            auto iter = original_textures.find(sprite.sprite->texture().get());
            if (sprite.dest_texture >= texture_infos.size() || iter == original_textures.cend())
            {
                debug_fail_ret_val(false);
            }

            ::original_texture_info& original_info = iter->second;
            ff::rect_size source_size = sprite.source_rect.cast<size_t>();
            sprite.dest_sprite_type = ff::dxgi::get_sprite_type(*original_info.rgb_scratch, &source_size);

            const DirectX::Image& source_image = *original_info.rgb_scratch->GetImages();
            const DirectX::Image& dest_image = *texture_infos[sprite.dest_texture].scratch_texture.GetImages();
            ff::rect_int dest_rect = sprite.dest_rect;
            bool has_border = sprite.source_rect.size() != dest_rect.size();

            if (has_border)
            {
                dest_rect = dest_rect.deflate(::BORDER_SIZE, ::BORDER_SIZE);
            }

            bool status = SUCCEEDED(DirectX::CopyRectangle(
                source_image,
                DirectX::Rect(
                    sprite.source_rect.left,
                    sprite.source_rect.top,
                    sprite.source_rect.width(),
                    sprite.source_rect.height()),
                dest_image,
                DirectX::TEX_FILTER_DEFAULT,
                dest_rect.left,
                dest_rect.top));

            // Top row
            status &= !has_border ||
                SUCCEEDED(DirectX::CopyRectangle(
                    source_image,
                    DirectX::Rect(
                        sprite.source_rect.left,
                        sprite.source_rect.top,
                        sprite.source_rect.width(),
                        1),
                    dest_image,
                    DirectX::TEX_FILTER_DEFAULT,
                    dest_rect.left,
                    dest_rect.top - 1));

            // Bottom row
            status &= !has_border ||
                SUCCEEDED(DirectX::CopyRectangle(
                    source_image,
                    DirectX::Rect(
                        sprite.source_rect.left,
                        sprite.source_rect.bottom - 1,
                        sprite.source_rect.width(),
                        1),
                    dest_image,
                    DirectX::TEX_FILTER_DEFAULT,
                    dest_rect.left,
                    dest_rect.bottom));

            // Left side
            status &= !has_border ||
                SUCCEEDED(DirectX::CopyRectangle(
                    source_image,
                    DirectX::Rect(
                        sprite.source_rect.left,
                        sprite.source_rect.top,
                        1,
                        sprite.source_rect.height()),
                    dest_image,
                    DirectX::TEX_FILTER_DEFAULT,
                    dest_rect.left - 1,
                    dest_rect.top));

            // Right side
            status &= !has_border ||
                SUCCEEDED(DirectX::CopyRectangle(
                    source_image,
                    DirectX::Rect(
                        sprite.source_rect.right - 1,
                        sprite.source_rect.top,
                        1,
                        sprite.source_rect.height()),
                    dest_image,
                    DirectX::TEX_FILTER_DEFAULT,
                    dest_rect.right,
                    dest_rect.top));

            // Top left
            status &= !has_border ||
                SUCCEEDED(DirectX::CopyRectangle(
                    source_image,
                    DirectX::Rect(
                        sprite.source_rect.left,
                        sprite.source_rect.top,
                        1,
                        1),
                    dest_image,
                    DirectX::TEX_FILTER_DEFAULT,
                    dest_rect.left - 1,
                    dest_rect.top - 1));

            // Top right
            status &= !has_border ||
                SUCCEEDED(DirectX::CopyRectangle(
                    source_image,
                    DirectX::Rect(
                        sprite.source_rect.right - 1,
                        sprite.source_rect.top,
                        1,
                        1),
                    dest_image,
                    DirectX::TEX_FILTER_DEFAULT,
                    dest_rect.right,
                    dest_rect.top - 1));

            // Bottom left
            status &= !has_border ||
                SUCCEEDED(DirectX::CopyRectangle(
                    source_image,
                    DirectX::Rect(
                        sprite.source_rect.left,
                        sprite.source_rect.bottom - 1,
                        1,
                        1),
                    dest_image,
                    DirectX::TEX_FILTER_DEFAULT,
                    dest_rect.left - 1,
                    dest_rect.bottom));

            // Bottom right
            status &= !has_border ||
                SUCCEEDED(DirectX::CopyRectangle(
                    source_image,
                    DirectX::Rect(
                        sprite.source_rect.right - 1,
                        sprite.source_rect.bottom - 1,
                        1,
                        1),
                    dest_image,
                    DirectX::TEX_FILTER_DEFAULT,
                    dest_rect.right,
                    dest_rect.bottom));

            assert(status);
            return status;
        }));
    }

    bool status = true;
    for (ff::co_task_source<bool>& task : tasks)
    {
        status &= task.wait();
    }

    assert(status);
    return status;
}

static bool convert_final_textures(
    DXGI_FORMAT format,
    size_t mip_count,
    std::vector<::optimized_texture_info>& texture_infos,
    const std::shared_ptr<DirectX::ScratchImage>& palette_scratch)
{
    for (::optimized_texture_info& texure_info : texture_infos)
    {
        auto shared_scratch = std::make_shared<DirectX::ScratchImage>(std::move(texure_info.scratch_texture));
        auto dxgi_texture = ff::dxgi::create_static_texture(shared_scratch, ff::dxgi::sprite_type::unknown);
        std::shared_ptr<ff::texture> rgb_texture = std::make_shared<ff::texture>(dxgi_texture);
        texure_info.final_texture = std::make_shared<ff::texture>(*rgb_texture, format, mip_count);

        if (!*texure_info.final_texture)
        {
            debug_fail_ret_val(false);
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

std::vector<ff::sprite> ff::internal::optimize_sprites(const std::vector<ff::sprite>& old_sprites, DXGI_FORMAT new_format, size_t new_mip_count)
{
    std::vector<ff::sprite> new_sprites;
    assert_ret_val(new_mip_count == 1 || ff::dxgi::color_format(new_format), new_sprites);

    std::vector<::optimized_sprite_info> sprite_infos = ::create_sprite_infos(old_sprites);
    std::sort(sprite_infos.begin(), sprite_infos.end());

    std::unordered_map<const ff::texture*, ::original_texture_info> original_textures;
    std::shared_ptr<DirectX::ScratchImage> scratch_palette;
    std::vector<::optimized_texture_info> texture_infos;

    assert_ret_val(::create_original_textures(new_format, sprite_infos, original_textures, scratch_palette) &&
        ::compute_optimized_sprites(sprite_infos, texture_infos) &&
        ::create_optimized_textures(new_format, texture_infos), new_sprites);

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
    bool use_palette = ff::dxgi::palette_format(format);
    const int pixel_size = use_palette ? 1 : 4;
    const int alpha_offset = use_palette ? 0 : 3;

    for (::optimized_sprite_info& sprite_info : sprite_infos)
    {
        const ff::dxgi::sprite_data& sprite_data = sprite_info.sprite->sprite_data();
        const ff::texture* original_texture = sprite_info.sprite->texture().get();
        auto iter = original_textures.find(original_texture);
        assert_ret_val(iter != original_textures.cend(), false);

        const ::original_texture_info& texture_info = iter->second;
        const ff::rect_int& src_rect = sprite_info.source_rect;

        DirectX::ScratchImage outline_scratch;
        assert_hr_ret_val(outline_scratch.Initialize2D(
            use_palette ? DXGI_FORMAT_R8_UINT : DXGI_FORMAT_R8G8B8A8_UNORM,
            static_cast<size_t>(src_rect.width()) + 2,
            static_cast<size_t>(src_rect.height()) + 2,
            1, 1), false);

        std::memset(outline_scratch.GetPixels(), 0, outline_scratch.GetPixelsSize());

        const DirectX::Image& src_image = *texture_info.rgb_scratch->GetImages();
        const DirectX::Image& dest_image = *outline_scratch.GetImages();

        for (ff::point_int xy(src_rect.left, src_rect.top), dest_xy = {}; xy.y < src_rect.bottom; xy.y++, dest_xy.y++)
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

        auto dxgi_texture = ff::dxgi::create_static_texture(std::make_shared<DirectX::ScratchImage>(std::move(outline_scratch)), ff::dxgi::sprite_type::unknown);
        auto outline_texture = std::make_shared<ff::texture>(dxgi_texture, palette_data);
        outline_textures.push_back(outline_texture);

        outline_sprite_list.emplace_back(
            std::string(sprite_info.sprite->name()),
            outline_texture,
            ff::rect_float(ff::point_float{}, outline_texture->dxgi_texture()->size().cast<float>()),
            sprite_data.handle() + ff::point_float(1, 1),
            sprite_data.scale(),
            ff::dxgi::sprite_type::unknown);
    }

    return true;
}

std::vector<ff::sprite> ff::internal::outline_sprites(const std::vector<ff::sprite>& old_sprites, DXGI_FORMAT new_format, size_t new_mip_count)
{
    std::vector<ff::sprite> new_sprites;
    assert_ret_val(ff::dxgi::color_format(new_format) || ff::dxgi::palette_format(new_format), new_sprites);

    std::vector<::optimized_sprite_info> sprite_infos = ::create_sprite_infos(old_sprites);
    std::vector<std::shared_ptr<ff::texture>> outline_textures;
    std::unordered_map<const ff::texture*, ::original_texture_info> original_textures;
    std::shared_ptr<DirectX::ScratchImage> palette_data;

    if (!::create_original_textures(new_format, sprite_infos, original_textures, palette_data) ||
        !::create_outline_sprites(new_format, sprite_infos, original_textures, outline_textures, new_sprites, palette_data))
    {
        debug_fail_ret_val(new_sprites);
    }

    return ff::internal::optimize_sprites(new_sprites, new_format, new_mip_count);
}
