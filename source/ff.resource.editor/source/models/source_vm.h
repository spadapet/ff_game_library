#pragma once

namespace editor
{
    class source_vm : public ff::ui::notify_propety_changed_base
    {
    public:
        source_vm();
        source_vm(const std::filesystem::path& path);

        const char* full_path() const;
        const char* file_name() const;

        ff::dict save() const;

    private:
        std::string path_;
        std::string name_;

        NS_DECLARE_REFLECTION(editor::source_vm, ff::ui::notify_propety_changed_base);
    };
}
