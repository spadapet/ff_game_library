#pragma once

namespace editor
{
    class main_view_model : public ff::ui::notify_propety_changed_base
    {
    public:
        main_view_model();

    private:
        void file_exit_command(Noesis::BaseComponent* param);

        Noesis::Ptr<Noesis::ICommand> file_exit_command_;

        NS_DECLARE_REFLECTION(editor::main_view_model, ff::ui::notify_propety_changed_base);
    };

    class main_ui : public Noesis::UserControl
    {
    public:
        main_ui();

        editor::main_view_model* view_model() const;

    private:
        Noesis::Ptr<editor::main_view_model> view_model_;

        NS_DECLARE_REFLECTION(editor::main_ui, Noesis::UserControl);
    };
}
