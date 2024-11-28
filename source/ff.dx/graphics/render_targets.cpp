#include "pch.h"
#include "dxgi/draw_base.h"
#include "dxgi/draw_device_base.h"
#include "dxgi/interop.h"
#include "dxgi/palette_base.h"
#include "graphics/render_targets.h"
#include "graphics/texture_resource.h"
#include "types/color.h"
#include "types/transform.h"

static std::weak_ptr<ff::dxgi::texture_base> weak_texture_1080;
static std::weak_ptr<ff::dxgi::target_base> weak_target_1080;

static std::shared_ptr<ff::dxgi::texture_base> get_texture_1080()
{
    std::shared_ptr<ff::dxgi::texture_base> texture = ::weak_texture_1080.lock();
    if (!texture)
    {
        texture = ff::dxgi::create_render_texture(ff::point_size(1920, 1080), DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, &ff::color_none());
        ::weak_texture_1080 = texture;
    }

    return texture;
}

static std::shared_ptr<ff::dxgi::target_base> get_target_1080()
{
    std::shared_ptr<ff::dxgi::target_base> target = ::weak_target_1080.lock();
    if (!target)
    {
        target = ff::dxgi::create_target_for_texture(::get_texture_1080());
        ::weak_target_1080 = target;
    }

    return target;
}

ff::render_target::render_target(ff::point_size size, const DirectX::XMFLOAT4* clear_color, int palette_clear_color)
    : size(size)
    , viewport(size)
    , clear_color(clear_color ? *clear_color : ff::color_none())
    , palette_clear_color(static_cast<float>(palette_clear_color), 0, 0, 1)
    , used_targets{}
{}

ff::render_targets::render_targets(const std::shared_ptr<ff::dxgi::target_base>& default_target)
    : default_target_(default_target)
{}

void ff::render_targets::push(ff::render_target& entry)
{
    std::memset(entry.used_targets, 0, sizeof(entry.used_targets));
    this->entry_stack.push_back(&entry);
}

ff::rect_float ff::render_targets::pop(ff::dxgi::command_context_base& context, ff::dxgi::target_base* target, ff::dxgi::palette_base* palette)
{
    assert_ret_val(!this->entry_stack.empty(), ff::rect_float{});

    ff::render_target& entry = *this->entry_stack.back();
    this->entry_stack.pop_back();

    if (!target)
    {
        target = &this->target(context, ff::render_target_type::rgba);
    }

    const ff::point_size target_logical_size = target->size().logical_pixel_size;

    // Render to a 1080 intermediate target so that scaling to the final target size looks nicer with linear filtering.
    // If the final target is a multiple of 1080, then there is no need for the intermediate 1080.
    bool direct_to_target = (target_logical_size.x <= entry.size.x && target_logical_size.y <= entry.size.y) ||
        (target_logical_size.x / entry.size.x == target_logical_size.y / entry.size.y &&
            target_logical_size.x % entry.size.x == 0 && target_logical_size.y % entry.size.y == 0);

    if (!direct_to_target)
    {
        if (!this->texture_1080)
        {
            this->texture_1080 = std::make_shared<ff::texture>(::get_texture_1080());
            this->target_1080 = ::get_target_1080();
        }

        this->target_1080->clear(context, ff::color_none());
    }

    ff::rect_float target_rect({}, direct_to_target ? target_logical_size.cast<float>() : ff::point_float(1920, 1080));
    const ff::rect_float world_rect({}, entry.size.cast<float>());
    ff::dxgi::draw_ptr draw = direct_to_target
        ? ff::dxgi::global_draw_device().begin_draw(context, *target, nullptr, target_rect, world_rect)
        : ff::dxgi::global_draw_device().begin_draw(context, *this->target_1080, nullptr, target_rect, world_rect);
    if (draw)
    {
        constexpr size_t index_palette = static_cast<size_t>(ff::render_target_type::palette);
        constexpr size_t index_rgba = static_cast<size_t>(ff::render_target_type::rgba);
        constexpr size_t index_rgba_pma = static_cast<size_t>(ff::render_target_type::rgba_pma);

        if (entry.used_targets[index_palette])
        {
            assert(palette);
            if (palette)
            {
                draw->push_palette(palette);
                draw->draw_sprite(entry.textures[index_palette]->sprite_data(), ff::transform::identity());
                draw->pop_palette();
            }
        }

        if (entry.used_targets[index_rgba])
        {
            draw->draw_sprite(entry.textures[index_rgba]->sprite_data(), ff::transform::identity());
        }

        if (entry.used_targets[index_rgba_pma])
        {
            draw->push_pre_multiplied_alpha();
            draw->draw_sprite(entry.textures[index_rgba_pma]->sprite_data(), ff::transform::identity());
            draw->pop_pre_multiplied_alpha();
        }
    }

    if (!direct_to_target)
    {
        target_rect = entry.viewport.view(target_logical_size).cast<float>();
        draw = ff::dxgi::global_draw_device().begin_draw(context, *target, nullptr, target_rect, ff::rect_float(0, 0, 1920, 1080));
        if (draw)
        {
            draw->push_sampler_linear_filter(true);
            draw->draw_sprite(this->texture_1080->sprite_data(), ff::transform::identity());
            draw->pop_sampler_linear_filter();
        }
    }

    return target_rect;
}

const std::shared_ptr<ff::dxgi::target_base>& ff::render_targets::default_target() const
{
    return this->default_target_;
}

void ff::render_targets::default_target(const std::shared_ptr<ff::dxgi::target_base>& value)
{
    assert(value);
    this->default_target_ = value;
}

ff::dxgi::target_base& ff::render_targets::target(ff::dxgi::command_context_base& context, ff::render_target_type type)
{
    if (!this->entry_stack.empty())
    {
        ff::render_target& entry = *this->entry_stack.back();
        size_t index = static_cast<size_t>(type);

        if (!entry.used_targets[index])
        {
            const bool palette = (type == ff::render_target_type::palette);
            const DirectX::XMFLOAT4& clear_color = palette
                ? entry.palette_clear_color
                : (type == ff::render_target_type::rgba_pma ? ff::color_none() : entry.clear_color);

            if (!entry.textures[index])
            {
                DXGI_FORMAT format = palette ? DXGI_FORMAT_R8_UINT : DXGI_FORMAT_R8G8B8A8_UNORM;
                auto dxgi_texture = ff::dxgi::create_render_texture(entry.size, format, 1, 1, 1, &clear_color);
                entry.textures[index] = std::make_shared<ff::texture>(dxgi_texture);
            }

            if (!entry.targets[index])
            {
                entry.targets[index] = ff::dxgi::create_target_for_texture(entry.textures[index]->dxgi_texture());
            }

            entry.targets[index]->clear(context, clear_color);
            entry.used_targets[index] = true;
        }

        return *entry.targets[index];
    }

    return *this->default_target_;
}

ff::dxgi::depth_base& ff::render_targets::depth(ff::dxgi::command_context_base& context)
{
    ff::point_size size = !this->entry_stack.empty()
        ? this->entry_stack.back()->size
        : this->default_target_->size().physical_pixel_size();

    if (!this->depth_)
    {
        this->depth_ = ff::dxgi::create_depth(size);
    }
    else
    {
        this->depth_->physical_size(context, size);
    }

    return *this->depth_;
}
