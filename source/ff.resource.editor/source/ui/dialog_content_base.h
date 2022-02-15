#pragma once

namespace editor
{
    class dialog_content_base : public Noesis::UserControl
    {
    public:
        dialog_content_base();

        static const Noesis::DependencyProperty* title_property;
        static const Noesis::DependencyProperty* footer_property;
        static const Noesis::RoutedEvent* request_close_event;

    protected:
        virtual void on_ok();
        virtual void on_cancel();
        virtual void on_close_dialog();
        virtual bool apply_changes();

    private:
        void ok_command(Noesis::BaseComponent* param);
        void cancel_command(Noesis::BaseComponent* param);

        Noesis::UIElement::RoutedEvent_<Noesis::RoutedEventArgs> request_close();

        Noesis::Ptr<Noesis::BaseCommand> ok_command_;
        Noesis::Ptr<Noesis::BaseCommand> cancel_command_;

        NS_DECLARE_REFLECTION(editor::dialog_content_base, Noesis::UserControl);
    };
}
