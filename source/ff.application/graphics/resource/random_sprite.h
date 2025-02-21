#pragma once

#include "../resource/animation_base.h"
#include "../resource/animation_player_base.h"

namespace ff
{
    class random_sprite
        : public ff::animation_base
        , public ff::animation_player_base
        , public ff::resource_object_base
    {
    public:
        struct count_t
        {
            int count;
            int weight;
        };

        struct sprite_t
        {
            ff::auto_resource<ff::resource_object_base> source;
            std::shared_ptr<ff::animation_base> anim;
            int weight;
        };

        random_sprite(std::vector<count_t>&& repeat_counts, std::vector<count_t>&& sprite_counts, std::vector<sprite_t>&& sprites);
        random_sprite(random_sprite&& other) noexcept = default;
        random_sprite(const random_sprite& other) = default;

        random_sprite& operator=(random_sprite&& other) noexcept = default;
        random_sprite& operator=(const random_sprite& other) = default;

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
        const std::vector<std::pair<ff::animation_base*, float>>& pick_sprites() const;

        mutable std::vector<std::pair<ff::animation_base*, float>> picked;
        mutable size_t next_update;

        std::vector<count_t> repeat_counts;
        std::vector<count_t> sprite_counts;
        std::vector<sprite_t> sprites;
        int repeat_count_weight;
        int sprite_count_weight;
        int sprite_weight;
    };
}

namespace ff::internal
{
    class random_sprite_factory : public ff::resource_object_factory<random_sprite>
    {
    public:
        using ff::resource_object_factory<random_sprite>::resource_object_factory;

        virtual std::shared_ptr<resource_object_base> load_from_source(const ff::dict& dict, resource_load_context& context) const override;
        virtual std::shared_ptr<resource_object_base> load_from_cache(const ff::dict& dict) const override;
    };
}
