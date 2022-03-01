#include "pch.h"
#include "source/models/project_vm.h"

NS_IMPLEMENT_REFLECTION(editor::project_vm, "editor.project_vm")
{
}

editor::project_vm::project_vm()
{}

bool editor::project_vm::dirty() const
{
    return true; // false;
}
