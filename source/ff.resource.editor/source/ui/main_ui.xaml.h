#pragma once

namespace editor
{
    class main_vm;

    class main_ui : public Noesis::UserControl
    {
    public:
        main_ui();

        editor::main_vm* view_model() const;

    private:
        Noesis::Ptr<editor::main_vm> view_model_;

        NS_DECLARE_REFLECTION(editor::main_ui, Noesis::UserControl);
    };
}
