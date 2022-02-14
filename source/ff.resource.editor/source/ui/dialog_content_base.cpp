#include "pch.h"
#include "source/ui/dialog_content_base.h"

const Noesis::DependencyProperty* editor::dialog_content_base::title_property{};
const Noesis::DependencyProperty* editor::dialog_content_base::footer_property{};
const Noesis::RoutedEvent* editor::dialog_content_base::request_close_event{};

NS_IMPLEMENT_REFLECTION(editor::dialog_content_base, "editor.dialog_content_base")
{
    // Properties
    {
        Noesis::DependencyData* data = NsMeta<Noesis::DependencyData>(Noesis::TypeOf<SelfClass>());
        data->RegisterProperty<Noesis::String>(editor::dialog_content_base::title_property, "Title", Noesis::PropertyMetadata::Create(Noesis::String()));
        data->RegisterProperty<Noesis::Ptr<Noesis::UIElement>>(editor::dialog_content_base::footer_property, "Footer", Noesis::PropertyMetadata::Create<Noesis::Ptr<Noesis::UIElement>>({}));
    }

    // Events
    {
        Noesis::UIElementData* data = NsMeta<Noesis::UIElementData>(Noesis::TypeOf<SelfClass>());
        data->RegisterEvent(editor::dialog_content_base::request_close_event, "RequestClose", Noesis::RoutingStrategy_Bubble);
    }

    NsProp("ok_command", &editor::dialog_content_base::ok_command_);
    NsProp("cancel_command", &editor::dialog_content_base::cancel_command_);
}

editor::dialog_content_base::dialog_content_base()
    : ok_command_(Noesis::MakePtr<ff::ui::delegate_command>(Noesis::MakeDelegate(this, &editor::dialog_content_base::ok_command)))
    , cancel_command_(Noesis::MakePtr<ff::ui::delegate_command>(Noesis::MakeDelegate(this, &editor::dialog_content_base::cancel_command)))
{}

void editor::dialog_content_base::on_ok()
{
    if (this->apply_changes())
    {
        this->on_close_dialog();
    }
}

void editor::dialog_content_base::on_cancel()
{
    this->on_close_dialog();
}

void editor::dialog_content_base::on_close_dialog()
{
    Noesis::RoutedEventArgs args{ this, editor::dialog_content_base::request_close_event };
    this->RaiseEvent(args);

    assert_msg(args.handled, "Dialog couldn't be closed");
}

bool editor::dialog_content_base::apply_changes()
{
    return true;
}

void editor::dialog_content_base::ok_command(Noesis::BaseComponent* param)
{
    this->on_ok();
}

void editor::dialog_content_base::cancel_command(Noesis::BaseComponent* param)
{
    this->on_cancel();
}

Noesis::UIElement::RoutedEvent_<Noesis::RoutedEventArgs> editor::dialog_content_base::request_close()
{
    return Noesis::UIElement::RoutedEvent_<Noesis::RoutedEventArgs>(this, editor::dialog_content_base::request_close_event);
}
