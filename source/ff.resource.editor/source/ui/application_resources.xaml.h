#pragma once

namespace editor
{
    class application_resources : public Noesis::ResourceDictionary
    {
    public:
        application_resources();

    private:
        void on_click_ok_button(Noesis::BaseComponent* sender, const Noesis::RoutedEventArgs& args);
        void on_click_cancel_button(Noesis::BaseComponent* sender, const Noesis::RoutedEventArgs& args);

        NS_DECLARE_REFLECTION(editor::application_resources, Noesis::ResourceDictionary);
    };
}
