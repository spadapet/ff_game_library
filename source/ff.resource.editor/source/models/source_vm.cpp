#include "pch.h"
#include "source/models/source_vm.h"

NS_IMPLEMENT_REFLECTION(editor::source_vm, "editor.source_vm")
{
}

editor::source_vm::source_vm()
{}

const char* editor::source_vm::full_path() const
{
    return this->full_path_.c_str();
}

void editor::source_vm::full_path(const char* value)
{
    if (this->full_path_ != value)
    {
        this->full_path_ = value;
        this->property_changed("full_path");
        this->property_changed("file_name");
    }
}

const char* editor::source_vm::file_name() const
{
    size_t slash = this->full_path_.find_last_of('\\');
    if (slash != std::string::npos)
    {
        return &this->full_path_[slash + 1];
    }

    return full_path();
}
