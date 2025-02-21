#include "pch.h"
#include "graphics/dxgi/format_util.h"
#include "graphics/dxgi/sprite_data.h"
#include "graphics/dxgi/texture_base.h"
#include "graphics/dxgi/texture_view_base.h"

ff::dxgi::sprite_data::sprite_data()
    : view_(nullptr)
    , texture_uv_(0, 0, 0, 0)
    , world_(0, 0, 0, 0)
    , type_(ff::dxgi::sprite_type::unknown)
{}

ff::dxgi::sprite_data::sprite_data(
    ff::dxgi::texture_view_base* view,
    ff::rect_float texture_uv,
    ff::rect_float world,
    ff::dxgi::sprite_type type)
    : view_(view)
    , texture_uv_(texture_uv)
    , world_(world)
    , type_((type == ff::dxgi::sprite_type::unknown && view) ? view->view_texture()->sprite_type() : type)
{}

ff::dxgi::sprite_data::sprite_data(
    ff::dxgi::texture_view_base* view,
    ff::rect_float rect,
    ff::point_float handle,
    ff::point_float scale,
    ff::dxgi::sprite_type type)
    : view_(view)
    , texture_uv_(rect / view->view_texture()->size().cast<float>())
    , world_(-handle * scale, (rect.size() - handle) * scale)
    , type_((type == ff::dxgi::sprite_type::unknown && view) ? view->view_texture()->sprite_type() : type)
{}

ff::dxgi::sprite_data::operator bool() const
{
    return this->view_ != nullptr;
}

ff::dxgi::texture_view_base* ff::dxgi::sprite_data::view() const
{
    return this->view_;
}

const ff::rect_float& ff::dxgi::sprite_data::texture_uv() const
{
    return this->texture_uv_;
}

const ff::rect_float& ff::dxgi::sprite_data::world() const
{
    return this->world_;
}

ff::dxgi::sprite_type ff::dxgi::sprite_data::type() const
{
    return this->type_;
}

ff::rect_float ff::dxgi::sprite_data::texture_rect() const
{
    return (this->texture_uv_ * this->view_->view_texture()->size().cast<float>()).normalize();
}

ff::point_float ff::dxgi::sprite_data::scale() const
{
    return this->world_.size() / this->texture_rect().size();
}

ff::point_float ff::dxgi::sprite_data::handle() const
{
    return -this->world_.top_left() / this->scale();
}

ff::dxgi::sprite_type ff::dxgi::get_sprite_type(const DirectX::ScratchImage& scratch, const ff::rect_size* rect)
{
    DirectX::ScratchImage alpha_scratch;
    const DirectX::Image* alpha_image = nullptr;
    size_t alpha_gap = 1;
    DXGI_FORMAT format = scratch.GetMetadata().format;

    if (ff::dxgi::palette_format(format))
    {
        return ff::dxgi::sprite_type::opaque_palette;
    }
    else if (!ff::dxgi::has_alpha(format))
    {
        return ff::dxgi::sprite_type::opaque;
    }
    else if (format == DXGI_FORMAT_A8_UNORM)
    {
        alpha_image = scratch.GetImages();
    }
    else if (format == DXGI_FORMAT_R8G8B8A8_UNORM || format == DXGI_FORMAT_B8G8R8A8_UNORM)
    {
        alpha_image = scratch.GetImages();
        alpha_gap = 4;
    }
    else if (ff::dxgi::compressed_format(format))
    {
        if (FAILED(DirectX::Decompress(
            scratch.GetImages(),
            scratch.GetImageCount(),
            scratch.GetMetadata(),
            DXGI_FORMAT_A8_UNORM,
            alpha_scratch)))
        {
            assert(false);
            return ff::dxgi::sprite_type::unknown;
        }

        alpha_image = alpha_scratch.GetImages();
    }
    else
    {
        if (FAILED(DirectX::Convert(
            scratch.GetImages(), 1,
            scratch.GetMetadata(),
            DXGI_FORMAT_A8_UNORM,
            DirectX::TEX_FILTER_DEFAULT,
            0, alpha_scratch)))
        {
            assert(false);
            return ff::dxgi::sprite_type::unknown;
        }

        alpha_image = alpha_scratch.GetImages();
    }

    ff::dxgi::sprite_type newType = ff::dxgi::sprite_type::opaque;
    ff::rect_size size(0, 0, scratch.GetMetadata().width, scratch.GetMetadata().height);
    rect = rect ? rect : &size;

    for (size_t y = rect->top; y < rect->bottom && newType == ff::dxgi::sprite_type::opaque; y++)
    {
        const uint8_t* alpha = alpha_image->pixels + y * alpha_image->rowPitch + rect->left + alpha_gap - 1;
        for (size_t x = rect->left; x < rect->right; x++, alpha += alpha_gap)
        {
            if (*alpha && *alpha != 0xFF)
            {
                newType = ff::dxgi::sprite_type::transparent;
                break;
            }
        }
    }

    return newType;
}
