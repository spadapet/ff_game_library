#include "pch.h"
#include "source/models/project_vm.h"

NS_IMPLEMENT_REFLECTION(editor::project_vm, "editor.project_vm")
{
    NsProp("full_path", &editor::project_vm::full_path);
    NsProp("file_name", &editor::project_vm::file_name);
}

editor::project_vm::project_vm()
{}

bool editor::project_vm::dirty() const
{
    return true; // false;
}

const char* editor::project_vm::full_path() const
{
    return "c:\\fake\\project.txt";
}

const char* editor::project_vm::file_name() const
{
    return "project.txt";
}
