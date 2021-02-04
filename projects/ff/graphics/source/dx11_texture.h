#pragma once

#include "dx11_texture_view_base.h"
#include "animation_base.h"
#include "animation_player_base.h"
#include "graphics_child_base.h"
#include "sprite_base.h"

namespace ff
{
    class palette_data;
    enum class sprite_type;

    class dx11_texture_o
        : public ff::resource_object_base
        , public ff::internal::graphics_child_base
        , public ff::dx11_texture_view_base
        , public ff::sprite_base
        , public ff::animation_base
        , public ff::animation_player_base
    {
    public:
        dx11_texture_o(const std::filesystem::path& path, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN, size_t mip_count = 1);
        dx11_texture_o(ff::point_int size, DXGI_FORMAT format, size_t mip_count = 1, size_t array_size = 1, size_t sample_count = 1);
        dx11_texture_o(ff::point_int size, const std::shared_ptr<ff::palette_data>& palette, size_t array_size = 1, size_t sample_count = 1);
        dx11_texture_o(DirectX::ScratchImage&& data);
        dx11_texture_o(DirectX::ScratchImage&& data, const std::shared_ptr<ff::palette_data>& palette);
        dx11_texture_o(const dx11_texture_o& texture, DXGI_FORMAT new_format, size_t new_mip_count);
        dx11_texture_o(dx11_texture_o&& other) noexcept = default;
        dx11_texture_o(const dx11_texture_o& other) = delete;

        dx11_texture_o& operator=(dx11_texture_o&& other) noexcept = default;
        dx11_texture_o& operator=(const dx11_texture_o& other) = delete;

        ff::point_int size() const;
        size_t mip_count() const;
        size_t array_size() const;
        size_t sample_count() const;
        DXGI_FORMAT format() const;
        ff::sprite_type sprite_type() const;
        std::shared_ptr<ff::palette_data> palette() const;
        ID3D11Texture2D* texture() const;

        void update(size_t array_index, size_t mip_index, const ff::rect_int& rect, const void* data, DXGI_FORMAT data_format, bool update_local_cache);
        std::shared_ptr<DirectX::ScratchImage> capture(bool use_local_cache = true);

        // resource_object_base
        virtual ff::dict resource_get_siblings(const std::shared_ptr<resource>& self) const override;
        virtual bool resource_save_to_file(const std::filesystem::path& directory_path, std::string_view name) const override;

        // graphics_child_base
        virtual bool reset() override;

        // dx11_texture_view_base
        virtual ID3D11ShaderResourceView* view() const override;

        // sprite_base
        virtual const ff::sprite_data& sprite_data() const override;

        // animation_base
        virtual float frame_length() const override;
        virtual float frames_per_second() const override;
        virtual void frame_events(float start, float end, bool include_start, ff::push_back_base<ff::animation_event>& events) override;
        virtual void render_frame(ff::renderer_base& render, const ff::transform& transform, float frame, const ff::dict* params = nullptr) override;
        virtual ff::value_ptr frame_value(size_t value_id, float frame, const ff::dict* params = nullptr) override;

        // animation_player_base
        virtual void advance_animation(ff::push_back_base<ff::animation_event>* events) override;
        virtual void render_animation(ff::renderer_base& render, const ff::transform& transform) const override;
        virtual float animation_frame() const override;

    protected:
        virtual bool save_to_cache(ff::dict& dict, bool& allow_compress) const override;
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
