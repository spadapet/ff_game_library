#pragma once

namespace ff
{
    class animation_base;
    class renderer_base;
    struct animation_event;
    struct transform;

    class animation_player
    {
    public:
        animation_player(const std::shared_ptr<ff::animation_base>& animation, float start_frame, float speed, const ff::dict* params);
        animation_player(animation_player&& other) noexcept = default;
        animation_player(const animation_player& other) = default;

        animation_player& operator=(animation_player&& other) noexcept = default;
        animation_player& operator=(const animation_player & other) = default;

        void advance(ff::push_back_base<ff::animation_event>* events);
        void render(ff::renderer_base* render, const ff::transform& transform) const;
        float frame() const;
        const std::shared_ptr<ff::animation_base>& animation() const;

    private:
        ff::dict params;
        std::shared_ptr<ff::animation_base> animation_;
        float start;
        float frame_;
        float fps;
        float advances;
    };
}
