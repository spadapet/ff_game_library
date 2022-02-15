#include "pch.h"
#include "source/ui/dialog.xaml.h"

NS_IMPLEMENT_REFLECTION(editor::dialog, "editor.dialog")
{}

editor::dialog::dialog()
{
    this->Initialized() += Noesis::MakeDelegate(this, &editor::dialog::initialized);
    Noesis::GUI::LoadComponent(this, "dialog.xaml");
}

void editor::dialog::initialized(Noesis::BaseComponent* sender, const Noesis::EventArgs& args)
{
    Noesis::FocusManager::SetIsFocusScope(this, true);
}
