#pragma once

#include "animation_base.h"
#include "animation_player_base.h"
#include "sprite_base.h"
#include "sprite_data.h"

namespace ff
{
    class dx11_texture_view_base;

    class sprite_resource
        : public ff::sprite_base
        , public ff::animation_base
        , public ff::animation_player_base
        , public ff::resource_object_base
    {
    public:
        sprite_resource(std::string&& name, const std::shared_ptr<ff::resource>& source);
        sprite_resource(sprite_resource&& other) noexcept = default;
        sprite_resource(const sprite_resource& other) = default;

        sprite_resource& operator=(sprite_resource&& other) noexcept = default;
        sprite_resource& operator=(const sprite_resource& other) = default;

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

        // resource_object_base
        virtual bool resource_load_complete(bool from_source) override;
        virtual std::vector<std::shared_ptr<resource>> resource_get_dependencies() const override;

    protected:
        virtual bool save_to_cache(ff::dict& dict, bool& allow_compress) const override;

    private:
        std::string name_;
        ff::auto_resource<ff::sprite_base> source;
        const ff::sprite_data* sprite_data_;
    };
}

namespace ff::internal
{
    class sprite_resource_factory : public ff::resource_object_factory<sprite_resource>
    {
    public:
        using ff::resource_object_factory<sprite_resource>::resource_object_factory;

        virtual std::shared_ptr<resource_object_base> load_from_source(const ff::dict& dict, resource_load_context& context) const override;
        virtual std::shared_ptr<resource_object_base> load_from_cache(const ff::dict& dict) const override;
    };
}
