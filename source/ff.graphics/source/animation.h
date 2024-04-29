#pragma once

#include "animation_base.h"
#include "animation_keys.h"

namespace ff::internal
{
    class animation_factory;
}

namespace ff
{
    class create_animation;

    class animation
        : public ff::animation_base
        , public ff::resource_object_base
    {
    public:
        animation();
        animation(animation&& other) noexcept = default;
        animation(const animation& other) = delete;

        animation& operator=(animation&& other) noexcept = default;
        animation& operator=(const animation & other) = delete;

        // animation_base
        virtual float frame_length() const override;
        virtual float frames_per_second() const override;
        virtual void frame_events(float start, float end, bool include_start, ff::push_base<ff::animation_event>& events) override;
        virtual void draw_frame(ff::dxgi::draw_base& draw, const ff::dxgi::transform& transform, float frame, const ff::dict* params = nullptr) override;
        virtual ff::value_ptr frame_value(size_t value_id, float frame, const ff::dict* params = nullptr) override;

    protected:
        virtual bool save_to_cache(ff::dict& dict) const override;

    private:
        friend class ff::internal::animation_factory;

        struct visual_info
        {
            bool operator<(const ff::animation::visual_info& other) const;

            float start;
            float length;
            float speed;
            ff::animation_keys::method_t method;
            const ff::animation_keys* visual_keys;
            const ff::animation_keys* color_keys;
            const ff::animation_keys* position_keys;
            const ff::animation_keys* scale_keys;
            const ff::animation_keys* rotate_keys;
        };

        struct event_info
        {
            bool operator<(const ff::animation::event_info& other) const;

            float frame;
            std::string event_name;
            ff::dict params;
            ff::animation_event public_event;
        };

        std::vector<ff::value_ptr> save_events_to_cache() const;
        std::vector<ff::value_ptr> save_visuals_to_cache() const;
        ff::dict save_keys_to_cache() const;
        bool load(const ff::dict& dict, bool from_source, ff::resource_load_context& context);
        bool load_events(const std::vector<ff::value_ptr>& values, bool from_source, ff::resource_load_context& context);
        bool load_visuals(const std::vector<ff::value_ptr>& values, bool from_source, ff::resource_load_context& context);
        bool load_keys(const ff::dict& values, bool from_source, ff::resource_load_context& context);

        using cached_visuals_t = typename std::vector<std::shared_ptr<ff::animation_base>>;
        const ff::animation::cached_visuals_t* get_cached_visuals(const ff::value_ptr& value);

        float play_length_;
        float frame_length_;
        float frames_per_second_;
        ff::animation_keys::method_t method;
        std::vector<visual_info> visuals;
        std::vector<event_info> events;
        std::unordered_map<size_t, ff::animation_keys, ff::no_hash<size_t>> keys;
        mutable std::unordered_map<ff::value_ptr, ff::animation::cached_visuals_t> cached_visuals;
    };

    class create_animation
    {
    public:
        create_animation(float length, float frames_per_second = ff::constants::advances_per_second_f, ff::animation_keys::method_t method = ff::animation_keys::method_t::default_);
        create_animation(create_animation&& other) noexcept = default;
        create_animation(const create_animation& other) = default;

        create_animation& operator=(create_animation&& other) noexcept = default;
        create_animation& operator=(const create_animation & other) = default;

        void add_keys(const ff::create_animation_keys& key);
        void add_event(float frame, std::string_view name, const ff::dict* params = nullptr);
        void add_visual(
            float start,
            float length,
            float speed,
            ff::animation_keys::method_t method,
            std::string_view visual_keys,
            std::string_view color_keys,
            std::string_view position_keys,
            std::string_view scale_keys,
            std::string_view rotate_keys);

        std::shared_ptr<ff::animation> create() const;

    private:
        ff::dict dict;
        ff::dict keys;
        std::vector<ff::value_ptr> events;
        std::vector<ff::value_ptr> visuals;
    };
}

namespace ff::internal
{
    class animation_factory : public ff::resource_object_factory<animation>
    {
    public:
        using ff::resource_object_factory<animation>::resource_object_factory;

        virtual std::shared_ptr<resource_object_base> load_from_source(const ff::dict& dict, resource_load_context& context) const override;
        virtual std::shared_ptr<resource_object_base> load_from_cache(const ff::dict& dict) const override;
    };
}
