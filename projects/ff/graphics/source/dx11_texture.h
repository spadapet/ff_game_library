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
    class dx11_texture_o
        : public ff::internal::graphics_child_base
        , public ff::resource_object_base
        , public ff::texture_metadata_base
        , public ff::dx11_texture_view_base
        , public ff::sprite_base
        , public ff::animation_base
        , public ff::animation_player_base
    {
    public:
        dx11_texture_o(const std::filesystem::path& path, DXGI_FORMAT new_format = DXGI_FORMAT_UNKNOWN, size_t new_mip_count = 1);
        dx11_texture_o(ff::point_int size, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM, size_t mip_count = 1, size_t array_size = 1, size_t sample_count = 1);
        dx11_texture_o(const std::shared_ptr<DirectX::ScratchImage>& data, const std::shared_ptr<DirectX::ScratchImage>& palette = nullptr, ff::sprite_type sprite_type = ff::sprite_type::unknown);
        dx11_texture_o(const dx11_texture_o& other, DXGI_FORMAT new_format, size_t new_mip_count);
        dx11_texture_o(dx11_texture_o&& other) noexcept;
        dx11_texture_o(const dx11_texture_o& other) = delete;
        virtual ~dx11_texture_o() override;

        dx11_texture_o& operator=(dx11_texture_o&& other) noexcept;
        dx11_texture_o& operator=(const dx11_texture_o& other) = delete;
        operator bool() const;

        ff::sprite_type sprite_type() const;
        ID3D11Texture2D* texture();

        void update(size_t array_index, size_t mip_index, const ff::rect_int& rect, const void* data, DXGI_FORMAT data_format) const;
        std::shared_ptr<DirectX::ScratchImage> capture() const;

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
        virtual const dx11_texture_o* view_texture() const override;
        virtual ID3D11ShaderResourceView* view() override;

        // sprite_base
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

        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view_;
        std::shared_ptr<DirectX::ScratchImage> data_;
        std::shared_ptr<DirectX::ScratchImage> palette_;
        ff::sprite_data sprite_data_;
    };
}

namespace ff::internal
{
    class texture_factory : public ff::resource_object_factory<dx11_texture_o>
    {
    public:
        using ff::resource_object_factory<dx11_texture_o>::resource_object_factory;

        virtual std::shared_ptr<resource_object_base> load_from_source(const ff::dict& dict, resource_load_context& context) const override;
        virtual std::shared_ptr<resource_object_base> load_from_cache(const ff::dict& dict) const override;
    };
}
