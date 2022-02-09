#pragma once

namespace editor
{
    class source_vm : public ff::ui::notify_propety_changed_base
    {
    public:
        source_vm();

        const char* full_path() const;
        void full_path(const char* value);
        const char* file_name() const;

    private:
        std::string full_path_;
        NS_DECLARE_REFLECTION(editor::source_vm, ff::ui::notify_propety_changed_base);
    };
}
