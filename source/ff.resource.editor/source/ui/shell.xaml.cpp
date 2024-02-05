#include "pch.h"
#include "source/ui/shell.xaml.h"
#include "xaml.res.id.h"

NS_IMPLEMENT_REFLECTION(editor::shell, "editor.shell")
{
}

editor::shell::shell()
{
    Noesis::GUI::LoadComponent(this, assets::xaml::SHELL_XAML.data());
}
