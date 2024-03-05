#include "pch.h"
#include "source/models/plugin_vm.h"

NS_IMPLEMENT_REFLECTION(editor::plugin_vm, "editor.plugin_vm")
{}

editor::plugin_vm::plugin_vm()
{}

editor::plugin_vm::plugin_vm(const std::filesystem::path& path)
    : editor::file_vm(path)
{}

editor::plugin_vm::plugin_vm(const ff::dict& dict)
    : editor::file_vm(dict)
{}
