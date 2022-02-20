#include "pch.h"
#include "source/models/main_vm.h"
#include "source/models/project_vm.h"
#include "source/ui/dialog_content_base.h"
#include "source/ui/main_window.xaml.h"
#include "source/ui/save_project_dialog.xaml.h"

static editor::main_window* instance{};

NS_IMPLEMENT_REFLECTION(editor::main_window, "editor.main_window")
{
    NsProp("view_model", &editor::main_window::view_model);
}

editor::main_window::main_window()
    : view_model_(Noesis::MakePtr<editor::main_vm>())
{
    assert(!::instance);
    if (!::instance)
    {
        ::instance = this;
    }

    Noesis::GUI::LoadComponent(this, "main_window.xaml");
}

editor::main_window::~main_window()
{
    assert(::instance == this);
    if (::instance == this)
    {
        ::instance = nullptr;
    }
}

editor::main_window* editor::main_window::get()
{
    assert(::instance);
    return ::instance;
}

editor::main_vm* editor::main_window::view_model() const
{
    return this->view_model_;
}

bool editor::main_window::ConnectEvent(Noesis::BaseComponent* source, const char* event, const char* handler)
{
    if (source == this && "RequestClose"sv == event && "on_request_close_dialog"sv == handler)
    {
        this->AddHandler(editor::dialog_content_base::request_close_event, Noesis::MakeDelegate(this, &editor::main_window::on_request_close_dialog));
        return true;
    }

    return false;
}

bool editor::main_window::can_close()
{
    if (this->view_model_->has_modal_dialog())
    {
        // TODO: Flash modal dialog
        return false;
    }

    //if (this->view_model_->project()->dirty())
    {
        // TODO: memory load for dialog (6 ref counts at end, so probably circular reference)
        Noesis::Ptr<editor::save_project_dialog> dialog = Noesis::MakePtr<editor::save_project_dialog>();
        this->save_project_dialog_close_connection = dialog->dialog_closed().connect([](int result)
        {
            if (result != editor::dialog_content_base::RESULT_CANCEL)
            {
                ::PostMessage(*ff::window::main(), editor::window_base::WM_USER_FORCE_CLOSE, 0, 0);
            }
        });

        this->view_model_->push_modal_dialog(dialog);
        return false;
    }

    //return true;
}

void editor::main_window::on_request_close_dialog(Noesis::BaseComponent* sender, const Noesis::RoutedEventArgs& args)
{
    editor::dialog_content_base* dialog = Noesis::DynamicCast<editor::dialog_content_base*>(args.source);

    args.handled = dialog && this->view_model_->remove_modal_dialog(dialog);
    assert(args.handled);
}
