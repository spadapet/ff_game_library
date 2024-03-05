#pragma once

namespace editor
{
    class file_vm : public ff::ui::notify_property_changed_base
    {
    public:
        file_vm();
        file_vm(const std::filesystem::path& path);
        file_vm(const ff::dict& dict);

        const char* full_path() const;
        const char* file_name() const;

        const std::filesystem::path& path() const;
        virtual ff::dict save() const;

    private:
        std::filesystem::path original_path;
        std::string path_;
        std::string name_;

        NS_DECLARE_REFLECTION(editor::file_vm, ff::ui::notify_property_changed_base);
    };
}
