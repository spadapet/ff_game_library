#pragma once

#include "../graphics/animation_base.h"
#include "../graphics/animation_player_base.h"
#include "../graphics/sprite_base.h"

namespace ff
{
    class texture_view_base;

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
        virtual const ff::dxgi::sprite_data& sprite_data() const override;

        // animation_base
        virtual void draw_frame(ff::dxgi::draw_base& draw, const ff::transform& transform, float frame, const ff::dict* params = nullptr) override;

        // animation_player_base
        virtual void draw_animation(ff::dxgi::draw_base& draw, const ff::transform& transform) const override;

        // resource_object_base
        virtual bool resource_load_complete(bool from_source) override;
        virtual std::vector<std::shared_ptr<resource>> resource_get_dependencies() const override;

    protected:
        virtual bool save_to_cache(ff::dict& dict) const override;

    private:
        std::string name_;
        ff::auto_resource<ff::resource_object_base> source;
        const ff::dxgi::sprite_data* sprite_data_;
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
