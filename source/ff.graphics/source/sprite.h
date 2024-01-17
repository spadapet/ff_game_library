#pragma once

#include "animation_base.h"
#include "animation_player_base.h"
#include "sprite_base.h"
#include "texture.h"

namespace ff
{
    class sprite
        : public ff::sprite_base
        , public ff::animation_base
        , public ff::animation_player_base
    {
    public:
        sprite(std::string&& name, const std::shared_ptr<ff::texture>& texture, const ff::dxgi::sprite_data& sprite_data);
        sprite(std::string&& name, const std::shared_ptr<ff::texture>& texture, ff::rect_float rect, ff::point_float handle, ff::point_float scale, ff::dxgi::sprite_type type);
        sprite(sprite&& other) noexcept = default;
        sprite(const sprite& other) = default;

        sprite& operator=(sprite&& other) noexcept = default;
        sprite& operator=(const sprite& other) = default;

        const std::shared_ptr<ff::texture>& texture() const;

        // sprite_base
        virtual std::string_view name() const override;
        virtual const ff::dxgi::sprite_data& sprite_data() const override;

        // animation_base
        virtual void draw_frame(ff::dxgi::draw_base& draw, const ff::dxgi::transform& transform, float frame, const ff::dict* params = nullptr) override;

        // animation_player_base
        virtual void draw_animation(ff::dxgi::draw_base& draw, const ff::dxgi::transform& transform) const override;

    private:
        std::string name_;
        std::shared_ptr<ff::texture> texture_;
        ff::dxgi::sprite_data sprite_data_;
    };
}
