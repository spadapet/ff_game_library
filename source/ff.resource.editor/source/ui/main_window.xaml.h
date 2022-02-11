#pragma once

#include "source/ui/window_base.h"

namespace editor
{
    class main_vm;

    class main_window : public editor::window_base
    {
    public:
        main_window();
        virtual ~main_window() override;

        static editor::main_window* get();

    protected:
        virtual bool can_close() override;

    private:
        NS_DECLARE_REFLECTION(editor::main_window, editor::window_base);
    };
}
