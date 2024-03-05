#pragma once

#include "source/models/file_vm.h"

namespace editor
{
    class plugin_vm : public editor::file_vm
    {
    public:
        plugin_vm();
        plugin_vm(const std::filesystem::path& path);
        plugin_vm(const ff::dict& dict);

    private:
        NS_DECLARE_REFLECTION(editor::plugin_vm, editor::file_vm);
    };
}
