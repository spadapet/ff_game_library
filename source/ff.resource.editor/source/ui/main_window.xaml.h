#pragma once

#include "source/ui/window_base.h"

namespace editor
{
    class main_vm;

    class main_window : public editor::window_base
    {
    public:
        main_window();

    protected:
        virtual bool can_close() override;

    private:
        NS_DECLARE_REFLECTION(editor::main_window, editor::window_base);
    };
}
