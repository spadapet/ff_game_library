#pragma once

namespace editor
{
    class window_base : public Noesis::ContentControl
    {
    public:
        window_base();

        static const Noesis::DependencyProperty* title_property;
        static constexpr UINT WM_USER_FORCE_CLOSE = WM_USER + 1;

    protected:
        virtual void handle_message(ff::window_message& message);
        virtual bool can_close();

    private:
        ff::signal_connection message_connection;

        NS_DECLARE_REFLECTION(editor::window_base, Noesis::ContentControl);
    };
}
