#pragma once

#include "animation_base.h"
#include "animation_player_base.h"
#include "dx11_texture_view_base.h"
#include "graphics_child_base.h"
#include "sprite_base.h"
#include "sprite_data.h"

namespace ff
{
    class dx11_texture;

    class dx11_texture_view
        : public ff::internal::graphics_child_base
        , public ff::dx11_texture_view_base
        , public ff::sprite_base
        , public ff::animation_base
        , public ff::animation_player_base
    {
    public:
        dx11_texture_view(const std::shared_ptr<dx11_texture>& texture, size_t array_start = 0, size_t array_count = 0, size_t mip_start = 0, size_t mip_count = 0);
        dx11_texture_view(dx11_texture_view&& other) noexcept;
        dx11_texture_view(const dx11_texture_view& other) = delete;
        virtual ~dx11_texture_view() override;

        dx11_texture_view& operator=(dx11_texture_view&& other) noexcept;
        dx11_texture_view& operator=(const dx11_texture_view& other) = delete;
        operator bool() const;

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
        virtual void draw_frame(ff::draw_base& draw, const ff::transform& transform, float frame, const ff::dict* params = nullptr) override;
        virtual void draw_frame(ff::draw_base& draw, const ff::pixel_transform& transform, float frame, const ff::dict* params = nullptr) override;
        virtual ff::value_ptr frame_value(size_t value_id, float frame, const ff::dict* params = nullptr) override;

        // animation_player_base
        virtual void advance_animation(ff::push_base<ff::animation_event>* events) override;
        virtual void draw_animation(ff::draw_base& draw, const ff::transform& transform) const override;
        virtual void draw_animation(ff::draw_base& draw, const ff::pixel_transform& transform) const override;
        virtual float animation_frame() const override;
        virtual const ff::animation_base* animation() const override;

    private:
        void fix_sprite_data();

        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view_;
        std::shared_ptr<dx11_texture> texture_;
        ff::sprite_data sprite_data_;
        size_t array_start_;
        size_t array_count_;
        size_t mip_start_;
        size_t mip_count_;
    };
}
