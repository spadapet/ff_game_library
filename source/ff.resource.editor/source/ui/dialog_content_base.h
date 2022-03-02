#pragma once

namespace editor
{
    struct dialog_request_close_event_args : public Noesis::RoutedEventArgs
    {
        int result;

        dialog_request_close_event_args(int result, Noesis::BaseComponent* source, const Noesis::RoutedEvent* event)
            : Noesis::RoutedEventArgs(source, event)
            , result(result)
        {}
    };

    class dialog_content_base : public Noesis::UserControl
    {
    public:
        dialog_content_base();
        virtual ~dialog_content_base() override;

        Noesis::UIElement::RoutedEvent_<editor::dialog_request_close_event_args> request_close();

        ff::signal_sink<editor::dialog_content_base*, editor::dialog_content_base*>& hidden_by();
        ff::signal_sink<editor::dialog_content_base*, editor::dialog_content_base*, int>& revealed_by();
        ff::signal_sink<int, bool&>& apply_changes();
        ff::signal_sink<int>& dialog_closed();
        void add_connection(ff::signal_connection&& connection);

        virtual void on_hidden_by(editor::dialog_content_base* dialog);
        virtual void on_revealed_by(editor::dialog_content_base* dialog, int result);
        virtual bool on_window_close(ff::window* window);

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
        ff::signal<editor::dialog_content_base*, editor::dialog_content_base*> hidden_by_;
        ff::signal<editor::dialog_content_base*, editor::dialog_content_base*, int> revealed_by_;
        std::list<ff::signal_connection> connections;

        NS_DECLARE_REFLECTION(editor::dialog_content_base, Noesis::UserControl);
    };
}
