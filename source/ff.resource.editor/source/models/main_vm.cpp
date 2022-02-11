#include "pch.h"
#include "source/models/main_vm.h"
#include "source/models/project_vm.h"
#include "source/ui/main_window.xaml.h"
#include "source/ui/save_project_dialog.xaml.h"

static editor::main_vm* instance{};

NS_IMPLEMENT_REFLECTION(editor::main_vm, "editor.main_vm")
{
    NsProp("file_new_command", &editor::main_vm::file_new_command_);
    NsProp("file_open_command", &editor::main_vm::file_open_command_);
    NsProp("file_save_command", &editor::main_vm::file_save_command_);
    NsProp("file_save_as_command", &editor::main_vm::file_save_as_command_);
    NsProp("file_exit_command", &editor::main_vm::file_exit_command_);

    NsProp("project", &editor::main_vm::project);
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

bool editor::main_vm::can_close_project()
{
    //if (this->project_->dirty())
    //{
    //    return false;
    //}

    this->push_modal_dialog(Noesis::MakePtr<editor::save_project_dialog>());

    return !this->has_modal_dialog();
}

bool editor::main_vm::has_modal_dialog() const
{
    return !this->modal_dialogs.empty();
}

void editor::main_vm::push_modal_dialog(Noesis::FrameworkElement* dialog)
{
    assert(dialog && std::find(this->modal_dialogs.cbegin(), this->modal_dialogs.cend(), dialog) == this->modal_dialogs.cend());

    bool old_has = this->has_modal_dialog();
    this->modal_dialogs.push_back(Noesis::Ptr(dialog));

    if (old_has != this->has_modal_dialog())
    {
        this->property_changed("has_modal_dialog");
    }

    this->property_changed("modal_dialog");
}

void editor::main_vm::remove_modal_dialog(Noesis::FrameworkElement* dialog)
{
    auto i = std::find(this->modal_dialogs.cbegin(), this->modal_dialogs.cend(), dialog);
    assert(i != this->modal_dialogs.cend());

    if (i != this->modal_dialogs.cend())
    {
        bool old_has = this->has_modal_dialog();
        this->modal_dialogs.erase(i);

        if (old_has != this->has_modal_dialog())
        {
            this->property_changed("has_modal_dialog");
        }

        this->property_changed("modal_dialog");
    }
}

Noesis::FrameworkElement* editor::main_vm::modal_dialog() const
{
    return this->has_modal_dialog() ? this->modal_dialogs.back() : nullptr;
}

void editor::main_vm::file_new_command(Noesis::BaseComponent* param)
{
}

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
