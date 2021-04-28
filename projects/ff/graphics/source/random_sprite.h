#pragma once

#include "animation_base.h"
#include "animation_player_base.h"

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

        static void advance_all();

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

        // resource_object_base
        virtual bool resource_load_complete(bool from_source) override;
        virtual std::vector<std::shared_ptr<resource>> resource_get_dependencies() const override;

    protected:
        virtual bool save_to_cache(ff::dict& dict, bool& allow_compress) const override;

    private:
        const std::vector<std::pair<ff::animation_base*, float>>& pick_sprites() const;

        mutable std::vector<std::pair<ff::animation_base*, float>> picked;
        mutable size_t next_advance;

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
