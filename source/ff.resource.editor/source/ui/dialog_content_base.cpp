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

    NsProp("close_command", &editor::dialog_content_base::close_command_);
}

editor::dialog_content_base::dialog_content_base()
    : close_command_(Noesis::MakePtr<ff::ui::delegate_command>(
        Noesis::MakeDelegate(this, &editor::dialog_content_base::close_command),
        Noesis::MakeDelegate(this, &editor::dialog_content_base::close_command_enabled)))
{}

editor::dialog_content_base::~dialog_content_base()
{}

Noesis::UIElement::RoutedEvent_<Noesis::RoutedEventArgs> editor::dialog_content_base::request_close()
{
    return Noesis::UIElement::RoutedEvent_<Noesis::RoutedEventArgs>(this, editor::dialog_content_base::request_close_event);
}

ff::signal_sink<int, bool&>& editor::dialog_content_base::apply_changes()
{
    return this->apply_changes_;
}

ff::signal_sink<int>& editor::dialog_content_base::dialog_closed()
{
    return this->dialog_closed_;
}

bool editor::dialog_content_base::can_window_close_while_modal() const
{
    return true;
}

bool editor::dialog_content_base::has_close_command(int result)
{
    return result != editor::dialog_content_base::RESULT_NO;
}

void editor::dialog_content_base::request_close_dialog(int result)
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
    bool success = true;
    this->apply_changes_.notify(result, success);
    return success;
}

static int get_result(Noesis::BaseComponent* param)
{
    return (param && Noesis::Boxing::CanUnbox<int>(param))
        ? Noesis::Boxing::Unbox<int>(param)
        : editor::dialog_content_base::RESULT_CANCEL;
}

void editor::dialog_content_base::close_command(Noesis::BaseComponent* param)
{
    const int result = ::get_result(param);
    if (!result || this->apply_changes(result))
    {
        this->request_close_dialog(result);
    }
}

bool editor::dialog_content_base::close_command_enabled(Noesis::BaseComponent* param)
{
    return this->has_close_command(::get_result(param));
}
