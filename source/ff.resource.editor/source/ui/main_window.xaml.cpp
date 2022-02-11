#include "pch.h"
#include "source/models/main_vm.h"
#include "source/ui/main_window.xaml.h"

static editor::main_window* instance{};

NS_IMPLEMENT_REFLECTION(editor::main_window, "editor.main_window")
{
}

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

bool editor::main_window::can_close()
{
    return editor::main_vm::get()->can_close_project();
}
