#include "pch.h"
#include "source/models/main_vm.h"
#include "source/ui/main_ui.xaml.h"

NS_IMPLEMENT_REFLECTION(editor::main_ui, "editor.main_ui")
{
    NsProp("view_model", &editor::main_ui::view_model);
}

editor::main_ui::main_ui()
    : view_model_(Noesis::MakePtr<editor::main_vm>())
{
    Noesis::GUI::LoadComponent(this, "main_ui.xaml");
}

editor::main_vm* editor::main_ui::view_model() const
{
    return this->view_model_;
}
