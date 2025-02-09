#include "pch.h"

namespace
{
    class app_state : public ff::game::root_state_base
    {
    public:
        virtual bool clear_back_buffer() override
        {
            return true;
        }

        virtual std::shared_ptr<ff::state> advance_time() override
        {
            for (auto& obj : this->rects)
            {
                obj.advance();
            }

            if (ff::input::keyboard().pressing('T'))
            {
                for (auto& obj : this->tris)
                {
                    obj.advance();
                }
            }

            if (ff::input::keyboard().pressing('C'))
            {
                for (auto& obj : this->circles)
                {
                    obj.advance();
                }
            }

            return ff::game::root_state_base::advance_time();
        }

        virtual void render(ff::dxgi::command_context_base& context, ff::render_targets& targets) override
        {
            ff::dxgi::depth_base& depth = targets.depth(context);
            ff::dxgi::target_base& target = targets.target(context);
            const ff::point_float target_size = target.size().logical_scaled_size<float>();

            if (ff::dxgi::draw_ptr draw = ff::dxgi::global_draw_device().begin_draw(context, target, &depth))
            {
                for (rect_t& obj : this->rects)
                {
                    if (obj.x >= target_size.x)
                    {
                        obj.dead = true;
                    }
                    else
                    {
                        draw->draw_rectangle(ff::rect_float(obj.x, 0.0f, obj.x + obj.width, target_size.y), obj.color, obj.thickness);
                    }
                }

                if (ff::input::keyboard().pressing('T'))
                {
                    for (tri_t& obj : this->tris)
                    {
                        ff::dxgi::endpoint_t points[3] =
                        {
                            { obj.pos[0], &obj.color },
                            { obj.pos[1] },
                            { obj.pos[2] },
                        };

                        draw->draw_triangles(points);
                    }
                }

                if (ff::input::keyboard().pressing('C'))
                {
                    for (circle_t& obj : this->circles)
                    {
                        ff::dxgi::endpoint_t pos = { obj.pos, &obj.color, obj.radius };
                        draw->draw_circle(pos, obj.thickness);
                    }
                }
            }

            ff::game::root_state_base::render(context, targets);
        }

    private:
        struct rect_t
        {
            rect_t()
                : width(static_cast<float>(ff::math::random_range(40, 400)))
                , x(-this->width)
                , speed(ff::math::random_range(2.0f, 80.0f))
                , thickness(ff::math::random_bool() ? std::make_optional(ff::math::random_range(1.0f, 100.0f)) : std::nullopt)
                , color(
                    1.0f - ff::math::random_range(0.0f, 1.0f),
                    1.0f - ff::math::random_range(0.0f, 1.0f),
                    1.0f - ff::math::random_range(0.0f, 1.0f),
                    ff::math::random_bool() ? 1.0f : ff::math::random_range(0.0f, 1.0f))
            {}

            void advance()
            {
                if (this->dead)
                {
                    *this = rect_t();
                }
                else
                {
                    this->x += this->speed;
                }
            }

            float width;
            float x;
            float speed;
            std::optional<float> thickness;
            ff::color color;
            bool dead{};
        };

        struct tri_t
        {
            tri_t()
                : pos{
                    ff::point_float(ff::math::random_range(0.0f, 500.0f), ff::math::random_range(0.0f, 500.0f)),
                    ff::point_float(ff::math::random_range(0.0f, 500.0f), ff::math::random_range(0.0f, 500.0f)),
                    ff::point_float(ff::math::random_range(0.0f, 500.0f), ff::math::random_range(0.0f, 500.0f)),
                }
                , color(
                    1.0f - ff::math::random_range(0.0f, 1.0f),
                    1.0f - ff::math::random_range(0.0f, 1.0f),
                    1.0f - ff::math::random_range(0.0f, 1.0f),
                    ff::math::random_bool() ? 1.0f : ff::math::random_range(0.0f, 1.0f))
                , life(ff::math::random_range(15, 200))
            {}

            void advance()
            {
                if (this->life)
                {
                    this->life--;
                }
                else
                {
                    *this = tri_t();
                }
            }

            ff::point_float pos[3];
            ff::color color;
            size_t life;
        };

        struct circle_t
        {
            circle_t()
                : pos{ ff::math::random_range(0.0f, 500.0f), ff::math::random_range(0.0f, 500.0f) }
                , color(
                    1.0f - ff::math::random_range(0.0f, 1.0f),
                    1.0f - ff::math::random_range(0.0f, 1.0f),
                    1.0f - ff::math::random_range(0.0f, 1.0f),
                    ff::math::random_bool() ? 1.0f : ff::math::random_range(0.0f, 1.0f))
                , radius(ff::math::random_range(4.0f, 300.0f))
                , thickness(ff::math::random_bool() ? std::make_optional(ff::math::random_range(1.0f, this->radius)) : std::nullopt)
                , life(ff::math::random_range(15, 200))
            {
            }

            void advance()
            {
                if (this->life)
                {
                    this->life--;
                }
                else
                {
                    *this = circle_t();
                }
            }

            ff::point_float pos;
            ff::color color;
            float radius;
            std::optional<float> thickness;
            size_t life;
        };

        std::array<rect_t, 16> rects;
        std::array<tri_t, 16> tris;
        std::array<circle_t, 16> circles;
    };
}

void run_test_game_wrapper()
{
    ff::game::run<::app_state>();
}
