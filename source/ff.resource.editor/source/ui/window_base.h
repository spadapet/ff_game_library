#pragma once

namespace editor
{
    class window_base : public Noesis::ContentControl
    {
    public:
        window_base();

    protected:
        virtual void handle_message(ff::window_message& message);
        virtual bool can_close();

    private:
        // XAML properties
        static const Noesis::DependencyProperty* title_property;

        ff::signal_connection message_connection;

        NS_DECLARE_REFLECTION(editor::window_base, Noesis::ContentControl);
    };
}
