#pragma once

#include "source/models/main_vm.h"
#include "source/ui/window_base.h"

namespace editor
{
    struct dialog_request_close_event_args;

    class main_window : public ff::ui::notify_property_changed_t<editor::window_base>
    {
    public:
        main_window();
        virtual ~main_window() override;

        static editor::main_window* get();
        editor::main_vm* view_model() const;

        virtual bool ConnectEvent(Noesis::BaseComponent* source, const char* event, const char* handler) override;

        template<class T>
        static ff::co_task<std::tuple<Noesis::Ptr<T>, int>> show_dialog()
        {
            Noesis::Ptr<T> dialog = Noesis::MakePtr<T>();
            editor::main_window::get()->view_model()->push_modal_dialog(dialog);
            int result = co_await dialog->awaitable();
            co_return std::make_tuple(dialog, result);
        }

    protected:
        virtual bool can_close() override;

    private:
        void on_request_close_dialog(Noesis::BaseComponent* sender, const editor::dialog_request_close_event_args& args);
        void modal_flash();

        Noesis::Ptr<editor::main_vm> view_model_;

        NS_DECLARE_REFLECTION(editor::main_window, ff::ui::notify_property_changed_t<editor::window_base>);
    };
}
