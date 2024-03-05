#include "pch.h"
#include "source/models/source_vm.h"

NS_IMPLEMENT_REFLECTION(editor::source_vm, "editor.source_vm")
{}

editor::source_vm::source_vm()
{}

editor::source_vm::source_vm(const std::filesystem::path& path)
    : editor::file_vm(path)
{}

editor::source_vm::source_vm(const ff::dict& dict)
    : editor::file_vm(dict)
{}
