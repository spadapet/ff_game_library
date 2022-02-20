#pragma once

#include "source/ui/window_base.h"

namespace editor
{
    class main_vm;

    class main_window : public editor::window_base
    {
    public:
        main_window();
        virtual ~main_window() override;

        static editor::main_window* get();
        editor::main_vm* view_model() const;

        virtual bool ConnectEvent(Noesis::BaseComponent* source, const char* event, const char* handler) override;

    protected:
        virtual bool can_close() override;

    private:
        void on_request_close_dialog(Noesis::BaseComponent* sender, const Noesis::RoutedEventArgs& args);

        ff::signal_connection save_project_dialog_close_connection;
        Noesis::Ptr<editor::main_vm> view_model_;

        NS_DECLARE_REFLECTION(editor::main_window, editor::window_base);
    };
}
