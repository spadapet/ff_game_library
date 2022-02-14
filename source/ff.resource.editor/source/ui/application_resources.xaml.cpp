#include "pch.h"
#include "source/ui/application_resources.xaml.h"

NS_IMPLEMENT_REFLECTION(editor::application_resources, "editor.application_resources")
{
}

editor::application_resources::application_resources()
{
    Noesis::GUI::LoadComponent(this, "application_resources.xaml");
}

void editor::application_resources::on_click_ok_button(Noesis::BaseComponent* sender, const Noesis::RoutedEventArgs& args)
{}

void editor::application_resources::on_click_cancel_button(Noesis::BaseComponent * sender, const Noesis::RoutedEventArgs & args)
{}
