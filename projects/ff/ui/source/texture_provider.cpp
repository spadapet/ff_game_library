#include "pch.h"
#include "resource_cache.h"
#include "texture.h"
#include "texture_provider.h"
#include "ui.h"

ff::internal::ui::texture_provider::texture_provider()
    : placeholder_texture(std::make_shared<ff::dx11_texture>(ff::point_int(1, 1)))
{}

Noesis::TextureInfo ff::internal::ui::texture_provider::GetTextureInfo(const char* uri)
{
    std::string name = std::string(uri) + ".metadata";
    ff::auto_resource<ff::texture_metadata> res = ff::internal::ui::global_resource_cache()->get_resource_object(name);
    std::shared_ptr<ff::texture_metadata> obj = res.object();

    if (obj)
    {
        ff::point_t<uint32_t> size = obj->size().cast<uint32_t>();
        return Noesis::TextureInfo{ size.x, size.y };
    }

    assert(false);
    return Noesis::TextureInfo{};
}

Noesis::Ptr<Noesis::Texture> ff::internal::ui::texture_provider::LoadTexture(const char* uri, Noesis::RenderDevice* device)
{
    std::string_view name(uri);
    std::shared_ptr<ff::resource> res = ff::internal::ui::global_resource_cache()->get_resource_object(name);
    Noesis::Ptr<ff::internal::ui::texture> texture = *new ff::internal::ui::texture(res, this->placeholder_texture, name);
    return texture;
}
