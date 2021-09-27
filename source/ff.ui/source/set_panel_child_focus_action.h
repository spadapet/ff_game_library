#pragma once

namespace ff::ui
{
    class set_panel_child_focus_action : public NoesisApp::TargetedTriggerActionT<Noesis::UIElement>
    {
    protected:
        Noesis::Ptr<Noesis::Freezable> CreateInstanceCore() const override;
        void Invoke(Noesis::BaseComponent* parameter) override;

        NS_DECLARE_REFLECTION(set_panel_child_focus_action, NoesisApp::TargetedTriggerActionT<Noesis::UIElement>)
    };
}
