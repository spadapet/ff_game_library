#include "pch.h"
#include "graphics/dxgi/format_util.h"
#include "graphics/resource/render_targets.h"

ff::render_targets::render_targets(
    ff::point_size size,
    DXGI_FORMAT format,
    size_t mip_count,
    size_t array_size,
    size_t sample_count,
    const ff::color* optimized_clear_color)
    : format_(format)
    , mip_count_(mip_count)
    , array_size_(array_size)
    , sample_count_(sample_count)
    , size_(size)
    , clear_color_(optimized_clear_color
        ? *optimized_clear_color
        : (ff::dxgi::palette_format(format)
            ? ff::color_none_palette()
            : ff::color_none()))
{
}

size_t ff::render_targets::count() const
{
    return this->targets.size();
}

void ff::render_targets::count(size_t value)
{
    this->targets.resize(value);
}

const ff::point_size ff::render_targets::size() const
{
    return this->size_;
}

void ff::render_targets::size(ff::dxgi::command_context_base& context, ff::point_size value)
{
    for (ff::render_targets::target_t& i : this->targets)
    {
        i.target.reset();
        i.texture.reset();
    }
}

DXGI_FORMAT ff::render_targets::format() const
{
    return this->format_;
}

size_t ff::render_targets::mip_count() const
{
    return this->mip_count_;
}

size_t ff::render_targets::array_size() const
{
    return this->array_size_;
}

size_t ff::render_targets::sample_count() const
{
    return this->sample_count_;
}

const ff::color& ff::render_targets::clear_color() const
{
    return this->clear_color_;
}

ff::dxgi::texture_base& ff::render_targets::texture(size_t index)
{
    ff::render_targets::target_t& data = this->targets[index];
    if (!data.texture)
    {
    }

    return *data.texture;
}

ff::dxgi::target_base& ff::render_targets::target(size_t index)
{
    ff::render_targets::target_t& data = this->targets[index];
    if (!data.target)
    {
    }

    return *data.target;
}

ff::dxgi::depth_base& ff::render_targets::depth(size_t index)
{
    ff::render_targets::target_t& data = this->targets[index];
    if (!data.depth)
    {
    }

    return *data.depth;
}
