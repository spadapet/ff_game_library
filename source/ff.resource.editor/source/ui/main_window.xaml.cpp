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
        Noesis::UIElement::RoutedEvent_<editor::dialog_content_base::request_close_handler> request_close(
            this, editor::dialog_content_base::request_close_event);
        request_close += Noesis::MakeDelegate(this, &editor::main_window::on_request_close_dialog);
        return true;
    }

    return false;
}

bool editor::main_window::can_close()
{
    if (this->view_model_->has_modal_dialog())
    {
        if (!this->view_model_->modal_dialog()->can_window_close())
        {
            this->modal_flash();
            return false;
        }
    }

    if (this->view_model_->project()->dirty())
    {
        Noesis::Ptr<editor::save_project_dialog> dialog = Noesis::MakePtr<editor::save_project_dialog>();
        dialog->add_connection(dialog->dialog_closed().connect([](int result)
            {
                if (result != editor::dialog_content_base::RESULT_CANCEL)
                {
                    ::PostMessage(*ff::window::main(), editor::window_base::WM_USER_FORCE_CLOSE, 0, 0);
                }
            }));

        this->view_model_->push_modal_dialog(dialog);
        return false;
    }

    return true;
}

void editor::main_window::on_request_close_dialog(Noesis::BaseComponent* sender, const editor::dialog_request_close_event_args& args)
{
    editor::dialog_content_base* dialog = Noesis::DynamicCast<editor::dialog_content_base*>(args.source);
    args.handled = dialog && this->view_model_->remove_modal_dialog(dialog, args.result);
}

void editor::main_window::modal_flash()
{
    Noesis::Storyboard* sb = this->FindResource<Noesis::Storyboard>("modal_flash_storyboard");
    assert(sb);

    if (sb)
    {
        sb->Begin();
    }
}
