#include "pch.h"
#include "texture.h"
#include "texture_provider.h"

ff::internal::ui::texture_provider::texture_provider(std::shared_ptr<ff::resource_object_provider> resources)
    : placeholder_texture(std::make_shared<ff::texture>(ff::dxgi_client().create_render_texture(ff::point_size(1, 1), DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, nullptr)))
    , resources(resources)
{}

Noesis::TextureInfo ff::internal::ui::texture_provider::GetTextureInfo(const Noesis::Uri& uri)
{
    Noesis::String uri_path;
    uri.GetPath(uri_path);

    std::string name = std::string(uri_path.Str(), uri_path.Size()) + ".metadata";
    ff::auto_resource<ff::texture_metadata> res = this->resources->get_resource_object(name);
    std::shared_ptr<ff::texture_metadata> obj = res.object();

    if (obj)
    {
        ff::point_t<uint32_t> size = obj->size().cast<uint32_t>();
        return Noesis::TextureInfo{ size.x, size.y };
    }

    assert(false);
    return Noesis::TextureInfo{};
}

Noesis::Ptr<Noesis::Texture> ff::internal::ui::texture_provider::LoadTexture(const Noesis::Uri& uri, Noesis::RenderDevice* device)
{
    Noesis::String uri_path;
    uri.GetPath(uri_path);
    std::string_view name(uri_path.Str(), uri_path.Size());

    std::shared_ptr<ff::resource> res = this->resources->get_resource_object(name);
    Noesis::Ptr<ff::internal::ui::texture> texture = *new ff::internal::ui::texture(res, this->placeholder_texture, name);
    return texture;
}
