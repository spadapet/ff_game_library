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

    NsProp("loaded_command", &editor::dialog_content_base::loaded_command_);
    NsProp("close_command", &editor::dialog_content_base::close_command_);
}

editor::dialog_content_base::dialog_content_base()
    : loaded_command_(Noesis::MakePtr<ff::ui::delegate_command>(Noesis::MakeDelegate(this, &editor::dialog_content_base::loaded_command)))
    , close_command_(Noesis::MakePtr<ff::ui::delegate_command>(
        Noesis::MakeDelegate(this, &editor::dialog_content_base::close_command),
        Noesis::MakeDelegate(this, &editor::dialog_content_base::close_command_enabled)))
{}

editor::dialog_content_base::~dialog_content_base()
{}

Noesis::UIElement::RoutedEvent_<editor::dialog_request_close_event_args> editor::dialog_content_base::request_close()
{
    return Noesis::UIElement::RoutedEvent_<editor::dialog_request_close_event_args>(this, editor::dialog_content_base::request_close_event);
}

ff::signal_sink<int, bool&>& editor::dialog_content_base::apply_changes()
{
    return this->apply_changes_;
}

ff::signal_sink<int>& editor::dialog_content_base::dialog_closed()
{
    return this->dialog_closed_;
}

void editor::dialog_content_base::add_connection(ff::signal_connection&& connection)
{
    this->connections.push_back(std::move(connection));
}

void editor::dialog_content_base::dialog_opened()
{}

void editor::dialog_content_base::dialog_closed(int result)
{
    this->dialog_closed_.notify(result);
    this->connections.clear();
}

bool editor::dialog_content_base::can_window_close()
{
    return true;
}

bool editor::dialog_content_base::has_close_command(int result)
{
    return result != editor::dialog_content_base::RESULT_NO;
}

bool editor::dialog_content_base::apply_changes(int result)
{
    return true;
}

static int get_result(Noesis::BaseComponent* param)
{
    return (param && Noesis::Boxing::CanUnbox<int>(param))
        ? Noesis::Boxing::Unbox<int>(param)
        : editor::dialog_content_base::RESULT_CANCEL;
}

void editor::dialog_content_base::loaded_command(Noesis::BaseComponent* param)
{
    Noesis::FrameworkElement* root = Noesis::DynamicCast<Noesis::FrameworkElement*>(param);
    if (root)
    {
        Noesis::TraversalRequest tr{ Noesis::FocusNavigationDirection_First };
        root->MoveFocus(tr);
    }
}

void editor::dialog_content_base::close_command(Noesis::BaseComponent* param)
{
    const int result = ::get_result(param);
    if (!result || this->apply_changes(result))
    {
        bool can_close = true;
        if (result)
        {
            this->apply_changes_.notify(result, can_close);
        }

        if (can_close)
        {
            this->request_close_dialog(result);
        }
    }
}

bool editor::dialog_content_base::close_command_enabled(Noesis::BaseComponent* param)
{
    return this->has_close_command(::get_result(param));
}

void editor::dialog_content_base::request_close_dialog(int result)
{
    Noesis::Ptr<Noesis::BaseComponent> keep_alive(this);
    editor::dialog_request_close_event_args args(result, this, editor::dialog_content_base::request_close_event);
    this->RaiseEvent(args);
    assert_ret(args.handled);
}
