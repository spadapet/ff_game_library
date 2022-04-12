#pragma once

#include "source/ui/dialog_content_base.h"

namespace editor
{
    class project_vm;

    class save_project_dialog : public editor::dialog_content_base
    {
    public:
        save_project_dialog();

        editor::project_vm* project() const;

        // dialog_content_base
        virtual bool can_window_close() override;

    protected:
        // dialog_content_base
        virtual bool has_close_command(int result) override;
        virtual bool apply_changes(int result) override;

    private:
        Noesis::Ptr<editor::project_vm> project_;

        NS_DECLARE_REFLECTION(editor::save_project_dialog, editor::dialog_content_base);
    };
}
