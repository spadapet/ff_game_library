#include "pch.h"
#include "source/models/file_vm.h"

NS_IMPLEMENT_REFLECTION(editor::file_vm, "editor.file_vm")
{
    NsProp("file_name", &editor::file_vm::file_name);
    NsProp("full_path", &editor::file_vm::full_path);
}

editor::file_vm::file_vm()
{}

editor::file_vm::file_vm(const std::filesystem::path& path)
    : original_path(path)
    , path_(ff::filesystem::to_string(path))
    , name_(ff::filesystem::to_string(path.filename()))
{}

editor::file_vm::file_vm(const ff::dict& dict)
    : original_path(ff::filesystem::to_path(dict.get<std::string>("path")))
    , path_(ff::filesystem::to_string(this->original_path))
    , name_(ff::filesystem::to_string(this->original_path.filename()))
{}

const char* editor::file_vm::full_path() const
{
    return this->path_.c_str();
}

const char* editor::file_vm::file_name() const
{
    return this->name_.c_str();
}

const std::filesystem::path& editor::file_vm::path() const
{
    return this->original_path;
}

ff::dict editor::file_vm::save() const
{
    ff::dict dict;
    dict.set<std::string>("path", this->path_);
    return dict;
}
