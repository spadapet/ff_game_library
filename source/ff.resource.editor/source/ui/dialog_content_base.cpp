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
    NsProp("close_command", &editor::dialog_content_base::close_command_);
}

editor::dialog_content_base::dialog_content_base()
    : ok_command_(Noesis::MakePtr<ff::ui::delegate_command>(Noesis::MakeDelegate(this, &editor::dialog_content_base::ok_command)))
    , cancel_command_(Noesis::MakePtr<ff::ui::delegate_command>(Noesis::MakeDelegate(this, &editor::dialog_content_base::cancel_command)))
    , close_command_(Noesis::MakePtr<ff::ui::delegate_command>(Noesis::MakeDelegate(this, &editor::dialog_content_base::close_command)))
{}

editor::dialog_content_base::~dialog_content_base()
{}

Noesis::UIElement::RoutedEvent_<Noesis::RoutedEventArgs> editor::dialog_content_base::request_close()
{
    return Noesis::UIElement::RoutedEvent_<Noesis::RoutedEventArgs>(this, editor::dialog_content_base::request_close_event);
}

ff::signal_sink<int>& editor::dialog_content_base::dialog_closed()
{
    return this->dialog_closed_;
}

void editor::dialog_content_base::on_ok()
{
    if (this->apply_changes(editor::dialog_content_base::RESULT_OK))
    {
        this->on_close_dialog(editor::dialog_content_base::RESULT_OK);
    }
}

void editor::dialog_content_base::on_cancel()
{
    this->on_close_dialog(editor::dialog_content_base::RESULT_CANCEL);
}

bool editor::dialog_content_base::can_window_close_while_modal() const
{
    return true;
}

void editor::dialog_content_base::on_close_dialog(int result)
{
    Noesis::Ptr<Noesis::BaseComponent> keep_alive(this);
    Noesis::RoutedEventArgs args{ this, editor::dialog_content_base::request_close_event };
    this->RaiseEvent(args);

    if (args.handled)
    {
        this->dialog_closed_.notify(result);
    }
}

bool editor::dialog_content_base::apply_changes(int result)
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

void editor::dialog_content_base::close_command(Noesis::BaseComponent* param)
{
    int result = (param && Noesis::Boxing::CanUnbox<int>(param))
        ? Noesis::Boxing::Unbox<int>(param)
        : editor::dialog_content_base::RESULT_CANCEL;

    if (result == editor::dialog_content_base::RESULT_CANCEL || this->apply_changes(result))
    {
        this->on_close_dialog(result);
    }
}
