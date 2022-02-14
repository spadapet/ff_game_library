#include "pch.h"
#include "source/ui/dialog.xaml.h"

NS_IMPLEMENT_REFLECTION(editor::dialog, "editor.dialog")
{
}

editor::dialog::dialog()
{
    Noesis::GUI::LoadComponent(this, "dialog.xaml");
}
