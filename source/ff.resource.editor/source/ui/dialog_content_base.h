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

        ff::co_task_source<int> awaitable() const;
        void dialog_opened();
        void dialog_closed(int result);
        virtual bool can_window_close();

        static const Noesis::DependencyProperty* title_property;
        static const Noesis::DependencyProperty* footer_property;
        static const Noesis::RoutedEvent* request_close_event;

        using request_close_handler = typename Noesis::Delegate<void(Noesis::BaseComponent*, const editor::dialog_request_close_event_args&)>;

        static const int RESULT_CANCEL = 0;
        static const int RESULT_OK = 1;
        static const int RESULT_NO = 2;

    protected:
        virtual bool has_close_command(int result);
        virtual ff::co_task<bool> apply_changes_async(int result);

    private:
        void loaded_command(Noesis::BaseComponent* param);
        void close_command(Noesis::BaseComponent* param);
        bool close_command_enabled(Noesis::BaseComponent* param);

        Noesis::Ptr<Noesis::BaseCommand> loaded_command_;
        Noesis::Ptr<Noesis::BaseCommand> close_command_;
        ff::co_task_source<int> task;

        NS_DECLARE_REFLECTION(editor::dialog_content_base, Noesis::UserControl);
    };
}
