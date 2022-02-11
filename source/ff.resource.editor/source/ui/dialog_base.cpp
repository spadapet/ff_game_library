#include "pch.h"
#include "source/ui/dialog_base.h"

const Noesis::DependencyProperty* editor::dialog_base::title_property{};
const Noesis::DependencyProperty* editor::dialog_base::footer_property{};

NS_IMPLEMENT_REFLECTION(editor::dialog_base, "editor.dialog_base")
{
    Noesis::DependencyData* data = NsMeta<Noesis::DependencyData>(Noesis::TypeOf<SelfClass>());
    data->RegisterProperty<Noesis::String>(editor::dialog_base::title_property, "Title", Noesis::PropertyMetadata::Create(Noesis::String()));
    data->RegisterProperty<Noesis::Ptr<Noesis::UIElement>>(editor::dialog_base::footer_property, "Footer", Noesis::PropertyMetadata::Create<Noesis::Ptr<Noesis::UIElement>>({}));
}

void editor::dialog_base::OnApplyTemplate()
{
    Noesis::Button* close_button = Noesis::DynamicCast<Noesis::Button*>(this->GetTemplateChild("PART_CloseButton"));
    if (close_button)
    {
        // close_button->Click() += 
    }
}
