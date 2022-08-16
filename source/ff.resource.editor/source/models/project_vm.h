#pragma once

#include "source/models/source_vm.h"

namespace editor
{
    class project_vm : public ff::ui::notify_propety_changed_base
    {
    public:
        project_vm();
        project_vm(const std::filesystem::path& path);

        bool dirty() const;

        const char* full_path() const;
        const char* full_path_raw() const;
        const char* file_name() const;
        const char* file_name_raw() const;

        bool save(const std::filesystem::path& path);

        Noesis::Ptr<editor::source_vm> add_source(const std::filesystem::path& source_path);
        void remove_source(editor::source_vm* source);

    private:
        Noesis::ObservableCollection<editor::source_vm> sources;
        std::string path_;
        std::string name_;
        bool dirty_;

        NS_DECLARE_REFLECTION(editor::project_vm, ff::ui::notify_propety_changed_base);
    };
}
