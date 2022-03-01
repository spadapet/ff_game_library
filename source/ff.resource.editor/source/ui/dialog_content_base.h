#pragma once

namespace editor
{
    class dialog_content_base : public Noesis::UserControl
    {
    public:
        dialog_content_base();
        virtual ~dialog_content_base() override;

        Noesis::UIElement::RoutedEvent_<Noesis::RoutedEventArgs> request_close();
        ff::signal_sink<int, bool&>& apply_changes();
        ff::signal_sink<int>& dialog_closed();

        virtual bool can_window_close_while_modal() const;

        static const Noesis::DependencyProperty* title_property;
        static const Noesis::DependencyProperty* footer_property;
        static const Noesis::RoutedEvent* request_close_event;
        static const int RESULT_CANCEL = 0;
        static const int RESULT_OK = 1;
        static const int RESULT_NO = 2;

    protected:
        virtual bool has_close_command(int result);
        virtual void request_close_dialog(int result);
        virtual bool apply_changes(int result);

    private:
        void close_command(Noesis::BaseComponent* param);
        bool close_command_enabled(Noesis::BaseComponent* param);

        Noesis::Ptr<Noesis::BaseCommand> close_command_;
        ff::signal<int, bool&> apply_changes_;
        ff::signal<int> dialog_closed_;

        NS_DECLARE_REFLECTION(editor::dialog_content_base, Noesis::UserControl);
    };
}
