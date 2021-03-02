#include "pch.h"
#include "texture.h"

ff::internal::ui::texture::texture(const std::shared_ptr<ff::resource>& resource, const std::shared_ptr<ff::dx11_texture>& placeholder_texture, std::string_view name)
    : resource(resource)
    , placeholder_texture(placeholder_texture)
    , name_(name)
{
    assert(this->resource.valid() && this->placeholder_texture);
}

ff::internal::ui::texture::texture(const std::shared_ptr<ff::dx11_texture>& texture, std::string_view name)
    : static_texture(texture)
    , name_(name)
{}

ff::internal::ui::texture* ff::internal::ui::texture::get(Noesis::Texture* texture)
{
    return static_cast<ff::internal::ui::texture*>(texture);
}

const std::string& ff::internal::ui::texture::name() const
{
    return this->name_;
}

const std::shared_ptr<ff::dx11_texture>& ff::internal::ui::texture::internal_texture() const
{
    auto texture = this->resource.valid() ? this->resource.object() : this->static_texture;
    return texture ? texture : this->placeholder_texture;

}

uint32_t ff::internal::ui::texture::GetWidth() const
{
    auto texture = this->internal_texture();
    return static_cast<uint32_t>(texture ? texture->size().x : 0);
}

uint32_t ff::internal::ui::texture::GetHeight() const
{
    auto texture = this->internal_texture();
    return static_cast<uint32_t>(texture ? texture->size().y : 0);
}

bool ff::internal::ui::texture::HasMipMaps() const
{
    auto texture = this->internal_texture();
    return texture && texture->mip_count() > 1;
}

bool ff::internal::ui::texture::IsInverted() const
{
    return false;
}
