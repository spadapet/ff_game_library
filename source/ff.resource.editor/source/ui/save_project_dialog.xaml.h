#pragma once

#include "source/ui/dialog_base.h"

namespace editor
{
    class save_project_dialog : public editor::dialog_base
    {
    public:
        save_project_dialog();

    private:
        NS_DECLARE_REFLECTION(editor::save_project_dialog, editor::dialog_base);
    };
}
