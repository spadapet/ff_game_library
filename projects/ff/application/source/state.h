#pragma once

namespace ff
{
    class state : public std::enable_shared_from_this<ff::state>
    {
    public:
        enum class cursor_t { default, hand };
        enum class advance_t { running, single_step, stopped };

        virtual ~state() = default;

        virtual std::shared_ptr<ff::state> advance_time();
        virtual void advance_input();
        virtual void render(ff::dx11_target_base& target, ff::dx11_depth& depth);
        virtual void render();

        virtual void frame_started(ff::state::advance_t type);
        virtual void frame_rendering(ff::state::advance_t type);
        virtual void frame_rendered(ff::state::advance_t type, ff::dx11_target_base& target, ff::dx11_depth& depth);

        virtual ff::state::cursor_t cursor();
        virtual std::shared_ptr<ff::state> wrap();
        virtual std::shared_ptr<ff::state> unwrap();

        virtual size_t child_state_count();
        virtual ff::state* child_state(size_t index);
    };
}
