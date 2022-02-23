#pragma once

namespace editor
{
    class properties : public Noesis::BaseComponent
    {
    public:
        static const Noesis::DependencyProperty* modal_flash_property;

    private:
        NS_DECLARE_REFLECTION(editor::properties, Noesis::BaseComponent);
    };
}
