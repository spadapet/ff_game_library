#pragma once

#include "dx11_texture_view_base.h"
#include "animation_base.h"
#include "animation_player_base.h"
#include "graphics_child_base.h"
#include "sprite_base.h"
#include "sprite_data.h"
#include "sprite_type.h"
#include "texture_metadata.h"

namespace ff
{
    class dx11_texture
        : public ff::internal::graphics_child_base
        , public ff::resource_object_base
        , public ff::texture_metadata_base
        , public ff::dx11_texture_view_base
        , public ff::sprite_base
        , public ff::animation_base
        , public ff::animation_player_base
    {
    public:
        dx11_texture(const ff::resource_file& resource_file, DXGI_FORMAT new_format = DXGI_FORMAT_UNKNOWN, size_t new_mip_count = 1);
        dx11_texture(ff::point_int size, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN, size_t mip_count = 1, size_t array_size = 1, size_t sample_count = 1);
        dx11_texture(const std::shared_ptr<DirectX::ScratchImage>& data, const std::shared_ptr<DirectX::ScratchImage>& palette = nullptr, ff::sprite_type sprite_type = ff::sprite_type::unknown);
        dx11_texture(const dx11_texture& other, DXGI_FORMAT new_format, size_t new_mip_count);
        dx11_texture(dx11_texture&& other) noexcept;
        dx11_texture(const dx11_texture& other) = delete;
        virtual ~dx11_texture() override;

        dx11_texture& operator=(dx11_texture&& other) noexcept;
        dx11_texture& operator=(const dx11_texture& other) = delete;
        operator bool() const;

        ff::sprite_type sprite_type() const;
        std::shared_ptr<DirectX::ScratchImage> data() const;
        std::shared_ptr<DirectX::ScratchImage> palette() const;
        ID3D11Texture2D* texture() const;

        bool update(size_t array_index, size_t mip_index, const ff::rect_int& rect, const void* data, DXGI_FORMAT data_format) const;

        // resource_object_base
        virtual ff::dict resource_get_siblings(const std::shared_ptr<resource>& self) const override;
        virtual bool resource_save_to_file(const std::filesystem::path& directory_path, std::string_view name) const override;

        // texture_metadata_base
        virtual ff::point_int size() const override;
        virtual size_t mip_count() const override;
        virtual size_t array_size() const override;
        virtual size_t sample_count() const override;
        virtual DXGI_FORMAT format() const override;

        // graphics_child_base
        virtual bool reset() override;

        // dx11_texture_view_base
        virtual const dx11_texture* view_texture() const override;
        virtual ID3D11ShaderResourceView* view() const override;

        // sprite_base
        virtual std::string_view name() const override;
        virtual const ff::sprite_data& sprite_data() const override;

        // animation_base
        virtual float frame_length() const override;
        virtual float frames_per_second() const override;
        virtual void frame_events(float start, float end, bool include_start, ff::push_base<ff::animation_event>& events) override;
        virtual void render_frame(ff::renderer_base& render, const ff::transform& transform, float frame, const ff::dict* params = nullptr) override;
        virtual ff::value_ptr frame_value(size_t value_id, float frame, const ff::dict* params = nullptr) override;

        // animation_player_base
        virtual void advance_animation(ff::push_base<ff::animation_event>* events) override;
        virtual void render_animation(ff::renderer_base& render, const ff::transform& transform) const override;
        virtual float animation_frame() const override;
        virtual const ff::animation_base* animation() const override;

    protected:
        virtual bool save_to_cache(ff::dict& dict, bool& allow_compress) const override;

    private:
        void fix_sprite_data(ff::sprite_type sprite_type);

        mutable Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_;
        mutable Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view_;
        std::shared_ptr<DirectX::ScratchImage> data_;
        std::shared_ptr<DirectX::ScratchImage> palette_;
        ff::sprite_data sprite_data_;
    };
}

namespace ff::internal
{
    class texture_factory : public ff::resource_object_factory<dx11_texture>
    {
    public:
        using ff::resource_object_factory<dx11_texture>::resource_object_factory;

        virtual std::shared_ptr<resource_object_base> load_from_source(const ff::dict& dict, resource_load_context& context) const override;
        virtual std::shared_ptr<resource_object_base> load_from_cache(const ff::dict& dict) const override;
    };
}
