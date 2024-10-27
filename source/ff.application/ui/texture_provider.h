#pragma once

namespace ff::internal::ui
{
    class texture_provider : public Noesis::TextureProvider
    {
    public:
        texture_provider(std::shared_ptr<ff::resource_object_provider> resources);

        virtual Noesis::TextureInfo GetTextureInfo(const Noesis::Uri& uri) override;
        virtual Noesis::Ptr<Noesis::Texture> LoadTexture(const Noesis::Uri& uri, Noesis::RenderDevice* device) override;

    private:
        std::shared_ptr<ff::texture> placeholder_texture;
        std::shared_ptr<ff::resource_object_provider> resources;
    };
}
