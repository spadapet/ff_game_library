#pragma once

#include "source/models/file_vm.h"

namespace editor
{
    class source_vm : public editor::file_vm
    {
    public:
        source_vm();
        source_vm(const std::filesystem::path& path);
        source_vm(const ff::dict& dict);

    private:
        NS_DECLARE_REFLECTION(editor::source_vm, editor::file_vm);
    };
}
