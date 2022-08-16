#include "pch.h"
#include "source/models/source_vm.h"

NS_IMPLEMENT_REFLECTION(editor::source_vm, "editor.source_vm")
{
    NsProp("file_name", &editor::source_vm::file_name);
    NsProp("full_path", &editor::source_vm::full_path);
}

editor::source_vm::source_vm()
{}

editor::source_vm::source_vm(const std::filesystem::path& path)
    : path_(ff::filesystem::to_string(path))
    , name_(ff::filesystem::to_string(path.filename()))
{}

const char* editor::source_vm::full_path() const
{
    return this->path_.c_str();
}

const char* editor::source_vm::file_name() const
{
    return this->name_.c_str();
}

ff::dict editor::source_vm::save() const
{
    ff::dict dict;
    dict.set<std::string>("path", this->path_);
    return dict;
}
