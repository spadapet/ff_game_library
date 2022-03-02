#include "pch.h"
#include "source/ui/save_project_dialog.xaml.h"

NS_IMPLEMENT_REFLECTION(editor::save_project_dialog, "editor.save_project_dialog")
{}

editor::save_project_dialog::save_project_dialog()
{
    Noesis::GUI::LoadComponent(this, "save_project_dialog.xaml");
}

bool editor::save_project_dialog::on_window_close(ff::window* window)
{
    return false;
}

bool editor::save_project_dialog::has_close_command(int result)
{
    return true;
}
