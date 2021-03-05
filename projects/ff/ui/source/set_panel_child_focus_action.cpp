#include "pch.h"
#include "set_panel_child_focus_action.h"

NS_IMPLEMENT_REFLECTION_(ff::ui::set_panel_child_focus_action, "ff.ui.SetPanelChildFocusAction")

Noesis::Ptr<Noesis::Freezable> ff::ui::set_panel_child_focus_action::CreateInstanceCore() const
{
    return *new set_panel_child_focus_action();
}

void ff::ui::set_panel_child_focus_action::Invoke(Noesis::BaseComponent*)
{
    Noesis::UIElement* element = this->GetTarget();

    if (element)
    {
        Noesis::Panel* panel = Noesis::DynamicCast<Noesis::Panel*>(element->GetUIParent());
        if (panel && panel->GetChildren())
        {
            for (int i = 0; i < panel->GetChildren()->Count(); i++)
            {
                Noesis::UIElement* child = panel->GetChildren()->Get(static_cast<unsigned int>(i));
                if (child->GetIsFocused())
                {
                    // No need to change focus
                    element = nullptr;
                    break;
                }
            }
        }
    }

    if (element)
    {
        element->Focus();
    }
}
