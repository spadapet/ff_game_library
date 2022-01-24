#pragma once

namespace ff::game
{
    class debug_state : public ff::state
    {
    public:
        bool visible();
        void visible(std::shared_ptr<ff::state> top_state, std::shared_ptr<ff::state> under_state = nullptr);
        void hide();

        // State
        virtual void render() override;
        virtual size_t child_state_count() override;
        virtual ff::state* child_state(size_t index) override;

    private:
        std::shared_ptr<ff::state> top_state;
        std::shared_ptr<ff::state> under_state;
    };
}
