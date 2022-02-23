#include "pch.h"
#include "source/ui/properties.h"

const Noesis::DependencyProperty* editor::properties::modal_flash_property{};

NS_IMPLEMENT_REFLECTION(editor::properties, "editor.properties")
{
    Noesis::DependencyData* data = NsMeta<Noesis::DependencyData>(Noesis::TypeOf<SelfClass>());

    data->RegisterProperty<bool>(editor::properties::modal_flash_property, "ModalFlash",
        Noesis::FrameworkPropertyMetadata::Create(false, Noesis::FrameworkPropertyMetadataOptions_Inherits));
}
