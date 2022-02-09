#include "pch.h"
#include "source/models/main_vm.h"
#include "source/ui/main_window.xaml.h"

NS_IMPLEMENT_REFLECTION(editor::main_window, "editor.main_window")
{
}

editor::main_window::main_window()
{
    Noesis::GUI::LoadComponent(this, "main_window.xaml");
}

bool editor::main_window::can_close()
{
    return editor::main_vm::get()->can_close_project();
}
