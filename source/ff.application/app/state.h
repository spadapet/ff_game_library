#pragma once

namespace ff
{
    class render_targets;
    class state_wrapper;

    class state : public std::enable_shared_from_this<ff::state>
    {
    public:
        enum class advance_t { running, single_step, stopped };

        virtual ~state() = default;

        virtual std::shared_ptr<ff::state> advance_time();
        virtual void advance_input();
        virtual void render(ff::dxgi::command_context_base& context, ff::render_targets& targets);

        virtual void frame_started(ff::state::advance_t type);
        virtual void frame_rendering(ff::state::advance_t type, ff::dxgi::command_context_base& context, ff::render_targets& targets);
        virtual void frame_rendered(ff::state::advance_t type, ff::dxgi::command_context_base& context, ff::render_targets& targets);

        virtual std::shared_ptr<ff::state_wrapper> wrap();
        virtual std::shared_ptr<ff::state> unwrap();

        virtual size_t child_state_count();
        virtual ff::state* child_state(size_t index);
    };

    class state_list : public ff::state
    {
    public:
        state_list() = default;
        state_list(std::initializer_list<std::shared_ptr<ff::state>> list);
        state_list(std::vector<std::shared_ptr<ff::state>>&& states);
        state_list(state_list&& other) noexcept = default;
        state_list(const state_list& other) = default;

        state_list& operator=(state_list&& other) noexcept = default;
        state_list& operator=(const state_list& other) = default;

        void push(std::shared_ptr<ff::state> state);

        virtual size_t child_state_count() override;
        virtual ff::state* child_state(size_t index) override;

    private:
        std::vector<std::shared_ptr<ff::state>> states;
    };

    class state_wrapper : public ff::state
    {
    public:
        state_wrapper();
        state_wrapper(state_wrapper&& other) noexcept = default;
        state_wrapper(const state_wrapper& other) = default;
        state_wrapper(std::shared_ptr<ff::state> state);

        state_wrapper& operator=(state_wrapper&& other) noexcept = default;
        state_wrapper& operator=(const state_wrapper& other) = default;
        state_wrapper& operator=(std::shared_ptr<ff::state> state);

        operator bool() const;
        void reset();

        virtual std::shared_ptr<ff::state> advance_time() override;
        virtual void advance_input() override;
        virtual void render(ff::dxgi::command_context_base& context, ff::render_targets& targets) override;

        virtual void frame_started(ff::state::advance_t type) override;
        virtual void frame_rendering(ff::state::advance_t type, ff::dxgi::command_context_base& context, ff::render_targets& targets) override;
        virtual void frame_rendered(ff::state::advance_t type, ff::dxgi::command_context_base& context, ff::render_targets& targets) override;

        virtual std::shared_ptr<ff::state_wrapper> wrap() override;
        virtual std::shared_ptr<ff::state> unwrap() override;

    private:
        std::shared_ptr<ff::state> state;
    };
}
