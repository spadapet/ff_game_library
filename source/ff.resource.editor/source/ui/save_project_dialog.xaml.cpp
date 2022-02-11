#include "pch.h"
#include "source/ui/save_project_dialog.xaml.h"

NS_IMPLEMENT_REFLECTION(editor::save_project_dialog, "editor.save_project_dialog")
{
}

editor::save_project_dialog::save_project_dialog()
{
    Noesis::GUI::LoadComponent(this, "save_project_dialog.xaml");
}
