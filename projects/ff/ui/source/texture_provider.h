#pragma once

namespace ff::internal::ui
{
    class texture_provider : public Noesis::TextureProvider
    {
    public:
        texture_provider();

        virtual Noesis::TextureInfo GetTextureInfo(const Noesis::Uri& uri) override;
        virtual Noesis::Ptr<Noesis::Texture> LoadTexture(const Noesis::Uri& uri, Noesis::RenderDevice* device) override;

    private:
        std::shared_ptr<ff::dx11_texture> placeholder_texture;
    };
}
