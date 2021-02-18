#pragma once

#include "animation_base.h"
#include "key_frames.h"

namespace ff::internal
{
    class animation_factory;
}

namespace ff
{
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
        operator bool() const;

        // animation_base
        virtual float frame_length() const override;
        virtual float frames_per_second() const override;
        virtual void frame_events(float start, float end, bool include_start, ff::push_base<ff::animation_event>& events) override;
        virtual void render_frame(ff::renderer_base& render, const ff::transform& transform, float frame, const ff::dict* params = nullptr) override;
        virtual ff::value_ptr frame_value(size_t value_id, float frame, const ff::dict* params = nullptr) override;

    protected:
        virtual bool save_to_cache(ff::dict& dict, bool& allow_compress) const override;

    private:
        friend class ff::internal::animation_factory;

        struct visual_info
        {
            bool operator<(const ff::animation::visual_info& other) const;

            float start;
            float length;
            float speed;
            ff::key_frames::method_t method;
            const ff::key_frames* visual_keys;
            const ff::key_frames* color_keys;
            const ff::key_frames* position_keys;
            const ff::key_frames* scale_keys;
            const ff::key_frames* rotate_keys;
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

        float frame_length_;
        float frames_per_second_;
        ff::key_frames::method_t method;
        std::vector<visual_info> visuals;
        std::vector<event_info> events;
        std::unordered_map<size_t, ff::key_frames, ff::no_hash<size_t>> keys;
        mutable std::unordered_map<ff::value_ptr, ff::animation::cached_visuals_t> cached_visuals;
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
