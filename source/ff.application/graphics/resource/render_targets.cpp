#include "pch.h"
#include "graphics/dxgi/dxgi_globals.h"
#include "graphics/dxgi/format_util.h"
#include "graphics/resource/render_targets.h"

ff::render_targets::render_targets(size_t count, ff::point_size size, double dpi_scale, DXGI_FORMAT format, size_t sample_count, const ff::color* optimized_clear_color)
    : format_(format)
    , sample_count_(sample_count)
    , size_(size)
    , dpi_scale_(dpi_scale)
    , clear_color_(optimized_clear_color
        ? *optimized_clear_color
        : (ff::dxgi::palette_format(format)
            ? ff::color_none_palette()
            : ff::color_none()))
{
    this->count(count);
}

size_t ff::render_targets::count() const
{
    return this->targets.size();
}

void ff::render_targets::count(size_t value)
{
    this->targets.resize(value);
}

ff::point_size ff::render_targets::size() const
{
    return this->size_;
}

double ff::render_targets::dpi_scale() const
{
    return this->dpi_scale_;
}

void ff::render_targets::size(ff::point_size value, double dpi_scale)
{
    if (this->size_ != value)
    {
        this->size_ = value;
        this->dpi_scale_ = dpi_scale;

        for (ff::render_targets::target_t& i : this->targets)
        {
            i.target.reset();
            i.texture.reset();
        }
    }
    else if (this->dpi_scale_ != dpi_scale)
    {
        this->dpi_scale_ = dpi_scale;

        for (ff::render_targets::target_t& i : this->targets)
        {
            i.texture.reset();
        }
    }
}

DXGI_FORMAT ff::render_targets::format() const
{
    return this->format_;
}

size_t ff::render_targets::sample_count() const
{
    return this->sample_count_;
}

const ff::color& ff::render_targets::clear_color() const
{
    return this->clear_color_;
}

void ff::render_targets::clear(ff::dxgi::command_context_base& context, size_t index)
{
    this->target(index).clear(context, this->clear_color_);
}

void ff::render_targets::discard(ff::dxgi::command_context_base& context, size_t index)
{
    this->target(index).discard(context);
}

ff::texture& ff::render_targets::texture(size_t index)
{
    ff::render_targets::target_t& data = this->targets[index];
    if (!data.texture)
    {
        auto dxgi_texture = ff::dxgi::create_render_texture(this->size_, this->format_, 1, 1, this->sample_count_, &this->clear_color_);
        data.texture = std::make_unique<ff::texture>(dxgi_texture);
    }

    return *data.texture;
}

ff::dxgi::target_base& ff::render_targets::target(size_t index)
{
    ff::render_targets::target_t& data = this->targets[index];
    if (!data.target)
    {
        data.target = ff::dxgi::create_target_for_texture(this->texture(index).dxgi_texture(), 0, 0, 0, 0, this->dpi_scale_);
    }

    return *data.target;
}

ff::dxgi::depth_base& ff::render_targets::depth(size_t index)
{
    ff::render_targets::target_t& data = this->targets[index];
    if (!data.depth)
    {
        data.depth = ff::dxgi::create_depth(this->size_, this->sample_count_);
    }

    return *data.depth;
}
