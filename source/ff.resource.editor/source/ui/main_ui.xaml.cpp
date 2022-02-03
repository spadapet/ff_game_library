#include "pch.h"
#include "source/ui/main_ui.xaml.h"

NS_IMPLEMENT_REFLECTION(editor::main_view_model, "editor.main_view_model")
{
    NsProp("file_exit_command", &editor::main_view_model::file_exit_command_);
}

editor::main_view_model::main_view_model()
    : file_exit_command_(Noesis::MakePtr<ff::ui::delegate_command>(Noesis::MakeDelegate(this, &editor::main_view_model::file_exit_command)))
{}

void editor::main_view_model::file_exit_command(Noesis::BaseComponent* param)
{
    ff::window::main()->close();
}

NS_IMPLEMENT_REFLECTION(editor::main_ui, "editor.main_ui")
{
    NsProp("view_model", &editor::main_ui::view_model);
}

editor::main_ui::main_ui()
    : view_model_(Noesis::MakePtr<editor::main_view_model>())
{
    Noesis::GUI::LoadComponent(this, "main_ui.xaml");
}

editor::main_view_model* editor::main_ui::view_model() const
{
    return this->view_model_;
}
