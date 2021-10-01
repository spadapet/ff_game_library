#pragma once

#include "animation_base.h"
#include "animation_player_base.h"
#include "sprite_base.h"
#include "sprite_data.h"
#include "texture_view_base.h"

#if DXVER == 11

namespace ff
{
    class texture;

    class texture_view
        : public ff::internal::dx11::device_child_base
        , public ff::texture_view_base
        , public ff::sprite_base
        , public ff::animation_base
        , public ff::animation_player_base
    {
    public:
        texture_view(const std::shared_ptr<ff::texture>& texture, size_t array_start = 0, size_t array_count = 0, size_t mip_start = 0, size_t mip_count = 0);
        texture_view(texture_view&& other) noexcept;
        texture_view(const texture_view& other) = delete;
        virtual ~texture_view() override;

        texture_view& operator=(texture_view&& other) noexcept;
        texture_view& operator=(const texture_view& other) = delete;
        operator bool() const;

        // graphics_child_base
        virtual bool reset() override;

        // texture_view_base
        virtual const ff::texture* view_texture() const override;
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

        mutable Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view_;
        std::shared_ptr<ff::texture> texture_;
        ff::sprite_data sprite_data_;
        size_t array_start_;
        size_t array_count_;
        size_t mip_start_;
        size_t mip_count_;
    };
}

#endif
