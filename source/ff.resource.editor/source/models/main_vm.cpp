#include "pch.h"
#include "source/models/main_vm.h"
#include "source/models/project_vm.h"

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

    return true;
}

bool editor::main_vm::has_modal_dialog() const
{
    return this->modal_dialog_ != nullptr;
}

void editor::main_vm::modal_dialog(Noesis::FrameworkElement* dialog)
{
    if (this->modal_dialog_ != dialog)
    {
        bool old_has = (this->modal_dialog_ != nullptr);
        bool new_has = (dialog != nullptr);

        this->modal_dialog_ = Noesis::Ptr(dialog);
        this->property_changed("modal_dialog");

        if (old_has != new_has)
        {
            this->property_changed("has_modal_dialog");
        }
    }
}

Noesis::FrameworkElement* editor::main_vm::modal_dialog() const
{
    return this->modal_dialog_;
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
