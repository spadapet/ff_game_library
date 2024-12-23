﻿#include "pch.h"

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
            for (obj_t& obj : this->objs)
            {
                obj.advance();
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
                for (obj_t& obj : this->objs)
                {
                    if (obj.x >= target_size.x)
                    {
                        obj.dead = true;
                    }
                    else
                    {
                        draw->draw_filled_rectangle(ff::rect_float(obj.x, 0.0f, obj.x + obj.width, target_size.y), obj.color);
                    }
                }
            }

            ff::game::root_state_base::render(context, targets);
        }

    private:
        struct obj_t
        {
            obj_t()
                : width(static_cast<float>(ff::math::random_range(40, 400)))
                , x(-this->width)
                , speed(ff::math::random_range(2.0f, 80.0f))
                , color(
                    1.0f - ff::math::random_range(0.0f, 1.0f),
                    1.0f - ff::math::random_range(0.0f, 1.0f),
                    1.0f - ff::math::random_range(0.0f, 1.0f),
                    1.0f)
            {}

            void advance()
            {
                if (this->dead)
                {
                    *this = obj_t();
                }
                else
                {
                    this->x += this->speed;
                }
            }

            float width;
            float x;
            float speed;
            DirectX::XMFLOAT4 color;
            bool dead{};
        };

        std::array<obj_t, 16> objs;
    };
}

void run_test_game_wrapper()
{
    ff::game::run<::app_state>();
}
