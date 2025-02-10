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
            if (!ff::input::keyboard().pressing('R'))
            {
                for (auto& obj : this->rects)
                {
                    obj.advance();
                }
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

            if (ff::input::keyboard().pressing('L'))
            {
                if (ff::input::keyboard().pressing(VK_SHIFT))
                {
                    for (auto& obj : this->line_strips)
                    {
                        obj.advance();
                    }
                }
                else
                {
                    for (auto& obj : this->lines)
                    {
                        obj.advance();
                    }
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

                if (ff::input::keyboard().pressing('L'))
                {
                    if (ff::input::keyboard().pressing(VK_SHIFT))
                    {
                        for (line_strip_t& obj : this->line_strips)
                        {
                            draw->draw_lines(obj.points);
                        }
                    }
                    else
                    {
                        for (line_t& obj : this->lines)
                        {
                            draw->draw_line(obj.start, obj.end, obj.color, obj.thickness);
                        }
                    }
                }
            }

            ff::game::root_state_base::render(context, targets);
        }

    private:
        static float rand(float min, float max)
        {
            return ff::math::random_range(min, max);
        }

        static ff::color rand_color()
        {
            return ff::color(rand(0, 1), rand(0, 1), rand(0, 1), ff::math::random_bool() ? 1.0f : rand(0, 1));
        }

        static ff::point_float rand_point()
        {
            return ff::point_float(rand(0, 800), rand(0, 600));
        }

        static size_t rand_life()
        {
            return ff::math::random_range(15, 200);
        }

        struct rect_t
        {
            rect_t()
                : width(rand(40, 400))
                , x(-this->width)
                , speed(rand(2, 80))
                , thickness(ff::math::random_bool() ? std::make_optional(rand(1, 100)) : std::nullopt)
                , color(rand_color())
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
                : pos{ rand_point(), rand_point(), rand_point() }
                , color(rand_color())
                , life(rand_life())
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
                : pos{ rand_point() }
                , color(rand_color())
                , radius(ff::math::random_range(4.0f, 300.0f))
                , thickness(ff::math::random_bool() ? std::make_optional(rand(1, this->radius)) : std::nullopt)
                , life(rand_life())
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

        struct line_t
        {
            line_t()
                : start(rand_point())
                , end(rand_point())
                , color(rand_color())
                , thickness(rand(1, 20))
                , life(rand_life())
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
                    *this = line_t();
                }
            }

            ff::point_float start;
            ff::point_float end;
            ff::color color;
            float thickness;
            size_t life;
        };

        struct line_strip_t
        {
            line_strip_t()
                : color(rand_color())
                , life(rand_life())
            {
                float thickness = rand(1, 20);
                size_t count = ff::math::random_range(3, 10);
                this->points.reserve(count);

                for (size_t i = 0; i < count; i++)
                {
                    this->points.emplace_back(rand_point(), &this->color, thickness);
                }
            }

            line_strip_t& operator=(line_strip_t&& other) noexcept
            {
                this->points = std::move(other.points);
                this->color = other.color;
                this->life = other.life;

                for (auto& i : this->points)
                {
                    i.color = &this->color;
                }

                return *this;
            }

            void advance()
            {
                if (this->life)
                {
                    this->life--;
                }
                else
                {
                    *this = line_strip_t();
                }
            }

            std::vector<ff::dxgi::endpoint_t> points;
            ff::color color;
            size_t life;
        };

        std::array<rect_t, 16> rects;
        std::array<tri_t, 16> tris;
        std::array<circle_t, 16> circles;
        std::array<line_t, 16> lines;
        std::array<line_strip_t, 16> line_strips;
    };
}

void run_test_game_wrapper()
{
    ff::game::run<::app_state>();
}
