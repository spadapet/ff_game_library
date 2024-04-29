#pragma once

#include "animation_base.h"
#include "animation_player_base.h"
#include "sprite_base.h"

namespace ff
{
    class texture
        : public ff::resource_object_base
        , public ff::sprite_base
        , public ff::animation_base
        , public ff::animation_player_base
    {
    public:
        texture(const ff::resource_file& resource_file, DXGI_FORMAT new_format = DXGI_FORMAT_UNKNOWN, size_t new_mip_count = 1);
        texture(const std::shared_ptr<ff::dxgi::texture_base>& dxgi_texture, const std::shared_ptr<DirectX::ScratchImage>& palette = nullptr);
        texture(const texture& other, DXGI_FORMAT new_format, size_t new_mip_count);
        texture(texture&& other) noexcept = default;
        texture(const texture& other) = delete;

        texture& operator=(texture&& other) noexcept = default;
        texture& operator=(const texture& other) = delete;
        operator bool() const;

        const std::shared_ptr<DirectX::ScratchImage>& palette() const;
        const std::shared_ptr<ff::dxgi::texture_base>& dxgi_texture() const;

        // resource_object_base
        virtual ff::dict resource_get_siblings(const std::shared_ptr<ff::resource>& self) const override;
        virtual bool resource_save_to_file(const std::filesystem::path& directory_path, std::string_view name) const override;

        // sprite_base
        virtual std::string_view name() const override;
        virtual const ff::dxgi::sprite_data& sprite_data() const override;

        // animation_base
        virtual void draw_frame(ff::dxgi::draw_base& draw, const ff::dxgi::transform& transform, float frame, const ff::dict* params = nullptr) override;

        // animation_player_base
        virtual void draw_animation(ff::dxgi::draw_base& draw, const ff::dxgi::transform& transform) const override;

    protected:
        virtual bool save_to_cache(ff::dict& dict) const override;

    private:
        void assign(const std::shared_ptr<ff::dxgi::texture_base>& dxgi_texture);

        std::shared_ptr<ff::dxgi::texture_base> dxgi_texture_;
        std::shared_ptr<DirectX::ScratchImage> palette_;
        ff::dxgi::sprite_data sprite_data_;
    };
}

namespace ff::internal
{
    class texture_factory : public ff::resource_object_factory<ff::texture>
    {
    public:
        using ff::resource_object_factory<ff::texture>::resource_object_factory;

        virtual std::shared_ptr<resource_object_base> load_from_source(const ff::dict& dict, resource_load_context& context) const override;
        virtual std::shared_ptr<resource_object_base> load_from_cache(const ff::dict& dict) const override;
    };
}
