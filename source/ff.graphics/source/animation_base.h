#pragma once

namespace ff::dxgi
{
    class draw_base;
    struct pixel_transform;
    struct transform;
}

namespace ff
{
    class animation_base;

    struct animation_event
    {
        animation_base* animation;
        const ff::dict* params;
        size_t event_id;
    };

    class animation_base
    {
    public:
        virtual ~animation_base() = default;

        virtual float frame_length() const = 0;
        virtual float frames_per_second() const = 0;
        virtual void frame_events(float start, float end, bool include_start, ff::push_base<ff::animation_event>& events) = 0;
        virtual void draw_frame(ff::dxgi::draw_base& draw, const ff::dxgi::transform& transform, float frame, const ff::dict* params = nullptr) = 0;
        virtual void draw_frame(ff::dxgi::draw_base& draw, const ff::dxgi::pixel_transform& transform, float frame, const ff::dict* params = nullptr) = 0;
        virtual ff::value_ptr frame_value(size_t value_id, float frame, const ff::dict* params = nullptr) = 0;
    };
}
