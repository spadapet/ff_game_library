#pragma once

#include "source/models/plugin_vm.h"
#include "source/models/source_vm.h"

namespace editor
{
    class project_vm : public ff::ui::notify_property_changed_base
    {
    public:
        project_vm();
        project_vm(const std::filesystem::path& path);

        bool dirty() const;

        const char* full_path() const;
        const char* full_path_raw() const;
        const char* file_name() const;
        const char* file_name_raw() const;

        ff::co_task<bool> save_async(bool save_as = false);
        ff::co_task<bool> save_async(const std::filesystem::path& path);

        Noesis::Ptr<editor::plugin_vm> add_plugin(const std::filesystem::path& path);
        bool remove_plugin(editor::plugin_vm* plugin);

        Noesis::Ptr<editor::source_vm> add_source(const std::filesystem::path& path);
        bool remove_source(editor::source_vm* source);

    private:
        void dirty(bool value);

        Noesis::Ptr<Noesis::ObservableCollection<editor::plugin_vm>> plugins;
        Noesis::Ptr<Noesis::ObservableCollection<editor::source_vm>> sources;
        std::string path_;
        std::string name_;
        bool dirty_{};

        NS_DECLARE_REFLECTION(editor::project_vm, ff::ui::notify_property_changed_base);
    };
}
