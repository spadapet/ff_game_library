#pragma once

namespace editor
{
    class main_ui;

    class main_state : public ff::state
    {
    public:
        main_state();
        main_state(main_state&& other) noexcept = default;
        main_state(const main_state& other) = delete;

        main_state& operator=(main_state&& other) noexcept = default;
        main_state& operator=(const main_state& other) = delete;

        virtual size_t child_state_count() override;
        virtual ff::state* child_state(size_t index) override;

    private:
        Noesis::Ptr<editor::main_ui> main_ui;
        std::shared_ptr<ff::ui_view_state> main_ui_state;
    };
}
