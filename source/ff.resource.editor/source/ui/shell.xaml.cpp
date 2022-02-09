#include "pch.h"
#include "source/ui/shell.xaml.h"

NS_IMPLEMENT_REFLECTION(editor::shell, "editor.shell")
{
}

editor::shell::shell()
{
    Noesis::GUI::LoadComponent(this, "shell.xaml");
}
