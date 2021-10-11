#pragma once

#include "animation_base.h"
#include "animation_player_base.h"
#include "sprite_base.h"
#include "sprite_data.h"

namespace ff
{
    class texture
        : public ff_dx::texture
        , public ff::resource_object_base
        , public ff::sprite_base
        , public ff::animation_base
        , public ff::animation_player_base
    {
    public:
        texture(const ff::resource_file& resource_file, DXGI_FORMAT new_format = DXGI_FORMAT_UNKNOWN, size_t new_mip_count = 1);
        texture(ff::point_int size, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN, size_t mip_count = 1, size_t array_size = 1, size_t sample_count = 1);
        texture(const std::shared_ptr<DirectX::ScratchImage>& data, const std::shared_ptr<DirectX::ScratchImage>& palette = nullptr, ff::dxgi::sprite_type sprite_type = ff::dxgi::sprite_type::unknown);
        texture(const texture& other, DXGI_FORMAT new_format, size_t new_mip_count);
        texture(texture&& other) noexcept;
        texture(const texture& other) = delete;

        static texture& get(ff::dxgi::texture_base& other);
        static const texture& get(const ff::dxgi::texture_base& other);
        texture& operator=(texture&& other) noexcept;
        texture& operator=(const texture& other) = delete;

        std::shared_ptr<DirectX::ScratchImage> palette() const;

        // resource_object_base
        virtual ff::dict resource_get_siblings(const std::shared_ptr<resource>& self) const override;
        virtual bool resource_save_to_file(const std::filesystem::path& directory_path, std::string_view name) const override;

        // sprite_base
        virtual std::string_view name() const override;
        virtual const ff::sprite_data& sprite_data() const override;

        // animation_base
        virtual float frame_length() const override;
        virtual float frames_per_second() const override;
        virtual void frame_events(float start, float end, bool include_start, ff::push_base<ff::animation_event>& events) override;
        virtual void draw_frame(ff::draw_base& draw, const ff::transform& transform, float frame, const ff::dict* params = nullptr) override;
        virtual void draw_frame(ff::draw_base& draw, const ff::pixel_transform& transform, float frame, const ff::dict* params = nullptr) override;
        virtual ff::value_ptr frame_value(size_t value_id, float frame, const ff::dict* params = nullptr) override;

        // animation_player_base
        virtual void advance_animation(ff::push_base<ff::animation_event>* events) override;
        virtual void draw_animation(ff::draw_base& draw, const ff::transform& transform) const override;
        virtual void draw_animation(ff::draw_base& draw, const ff::pixel_transform& transform) const override;
        virtual float animation_frame() const override;
        virtual const ff::animation_base* animation() const override;

    protected:
        virtual void on_reset() override;
        virtual bool save_to_cache(ff::dict& dict, bool& allow_compress) const override;

    private:
        std::shared_ptr<DirectX::ScratchImage> palette_;
        ff::sprite_data sprite_data_;
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
