#pragma once

namespace ff::internal::ui
{
    class texture : public Noesis::Texture
    {
    public:
        texture(const std::shared_ptr<ff::resource>& resource, const std::shared_ptr<ff::texture>& placeholder_texture, std::string_view name);
        texture(const std::shared_ptr<ff::texture>& texture, std::string_view name);

        static ff::internal::ui::texture* get(Noesis::Texture* texture);
        const std::string& name() const;
        std::shared_ptr<ff::texture> internal_texture() const;

        virtual uint32_t GetWidth() const override;
        virtual uint32_t GetHeight() const override;
        virtual bool HasMipMaps() const override;
        virtual bool IsInverted() const override;
        virtual bool HasAlpha() const override;

    private:
        mutable ff::auto_resource<ff::texture> resource;
        std::shared_ptr<ff::texture> static_texture;
        std::shared_ptr<ff::texture> placeholder_texture;
        std::string name_;

        NS_DECLARE_REFLECTION(ff::internal::ui::texture, Noesis::Texture);
    };
}
