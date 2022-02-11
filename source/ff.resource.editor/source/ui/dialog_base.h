#pragma once

#include "source/ui/window_base.h"

namespace editor
{
    class dialog_base : public Noesis::ContentControl
    {
    protected:
        virtual void OnApplyTemplate() override;

    private:
        // XAML properties
        static const Noesis::DependencyProperty* title_property;
        static const Noesis::DependencyProperty* footer_property;

        NS_DECLARE_REFLECTION(editor::dialog_base, Noesis::ContentControl);
    };
}
