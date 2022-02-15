#include "pch.h"
#include "source/models/main_vm.h"
#include "source/ui/dialog_content_base.h"
#include "source/ui/main_window.xaml.h"

static editor::main_window* instance{};

NS_IMPLEMENT_REFLECTION(editor::main_window, "editor.main_window")
{}

editor::main_window::main_window()
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
    return editor::main_vm::get()->can_close_project();
}

void editor::main_window::on_request_close_dialog(Noesis::BaseComponent* sender, const Noesis::RoutedEventArgs& args)
{
    editor::main_vm* vm = Noesis::DynamicCast<editor::main_vm*>(this->GetDataContext());
    editor::dialog_content_base* dialog = Noesis::DynamicCast<editor::dialog_content_base*>(args.source);

    if (vm && dialog)
    {
        vm->remove_modal_dialog(dialog);
        args.handled = true;
    }

    assert(args.handled);
}
