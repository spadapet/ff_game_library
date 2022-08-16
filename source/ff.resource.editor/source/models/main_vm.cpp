#include "pch.h"
#include "source/models/main_vm.h"
#include "source/models/project_vm.h"
#include "source/ui/dialog_content_base.h"
#include "source/ui/main_window.xaml.h"

static editor::main_vm* instance{};

NS_IMPLEMENT_REFLECTION(editor::main_vm, "editor.main_vm")
{
    NsProp("file_new_command", &editor::main_vm::file_new_command_);
    NsProp("file_open_command", &editor::main_vm::file_open_command_);
    NsProp("file_save_command", &editor::main_vm::file_save_command_);
    NsProp("file_save_as_command", &editor::main_vm::file_save_as_command_);
    NsProp("file_exit_command", &editor::main_vm::file_exit_command_);

    NsProp("project", &editor::main_vm::project, &editor::main_vm::project);
    NsProp("has_modal_dialog", &editor::main_vm::has_modal_dialog);
    NsProp("modal_dialog", &editor::main_vm::modal_dialog);
}

editor::main_vm::main_vm()
    : file_new_command_(Noesis::MakePtr<ff::ui::delegate_command>(Noesis::MakeDelegate(this, &editor::main_vm::file_new_command)))
    , file_open_command_(Noesis::MakePtr<ff::ui::delegate_command>(Noesis::MakeDelegate(this, &editor::main_vm::file_open_command)))
    , file_save_command_(Noesis::MakePtr<ff::ui::delegate_command>(Noesis::MakeDelegate(this, &editor::main_vm::file_save_command)))
    , file_save_as_command_(Noesis::MakePtr<ff::ui::delegate_command>(Noesis::MakeDelegate(this, &editor::main_vm::file_save_as_command)))
    , file_exit_command_(Noesis::MakePtr<ff::ui::delegate_command>(Noesis::MakeDelegate(this, &editor::main_vm::file_exit_command)))
    , project_(Noesis::MakePtr<editor::project_vm>())
{
    assert(!::instance);
    if (!::instance)
    {
        ::instance = this;
    }
}

editor::main_vm::~main_vm()
{
    assert(::instance == this);
    if (::instance == this)
    {
        ::instance = nullptr;
    }
}

editor::main_vm* editor::main_vm::get()
{
    assert(::instance);
    return ::instance;
}

editor::project_vm* editor::main_vm::project() const
{
    return this->project_;
}

void editor::main_vm::project(editor::project_vm* project_)
{
    Noesis::Ptr<editor::project_vm> new_project;
    if (!project_)
    {
        new_project = Noesis::MakePtr<editor::project_vm>();
        project_ = new_project;
    }

    if (this->project_ != project_)
    {
        this->project_ = Noesis::Ptr(project_);
    }
}

bool editor::main_vm::has_modal_dialog() const
{
    return !this->modal_dialogs.empty();
}

void editor::main_vm::push_modal_dialog(editor::dialog_content_base* dialog)
{
    assert(dialog && std::find(this->modal_dialogs.cbegin(), this->modal_dialogs.cend(), dialog) == this->modal_dialogs.cend());

    this->modal_dialogs.push_back(Noesis::Ptr(dialog));
    dialog->dialog_opened();

    this->property_changed("has_modal_dialog");
    this->property_changed("modal_dialog");
}

bool editor::main_vm::remove_modal_dialog(editor::dialog_content_base* dialog, int result)
{
    auto i = std::find(this->modal_dialogs.cbegin(), this->modal_dialogs.cend(), dialog);
    if (i != this->modal_dialogs.cend())
    {
        Noesis::Ptr<editor::dialog_content_base> dialog_keep_alive(dialog);
        this->modal_dialogs.erase(i);
        dialog->dialog_closed(result);

        this->property_changed("has_modal_dialog");
        this->property_changed("modal_dialog");

        return true;
    }

    debug_fail_ret_val(false);
}

editor::dialog_content_base* editor::main_vm::modal_dialog() const
{
    return this->has_modal_dialog() ? this->modal_dialogs.back() : nullptr;
}

void editor::main_vm::file_new_command(Noesis::BaseComponent* param)
{}

void editor::main_vm::file_open_command(Noesis::BaseComponent* param)
{}

void editor::main_vm::file_save_command(Noesis::BaseComponent* param)
{}

void editor::main_vm::file_save_as_command(Noesis::BaseComponent* param)
{}

void editor::main_vm::file_exit_command(Noesis::BaseComponent* param)
{
    ff::window::main()->close();
}
