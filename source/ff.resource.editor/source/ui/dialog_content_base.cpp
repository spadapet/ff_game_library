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
    , task(ff::co_task_source<int>::create())
{}

editor::dialog_content_base::~dialog_content_base()
{
    if (!this->task.done())
    {
        this->task.set_result(editor::dialog_content_base::RESULT_CANCEL);
    }
}

Noesis::UIElement::RoutedEvent_<editor::dialog_request_close_event_args> editor::dialog_content_base::request_close()
{
    return Noesis::UIElement::RoutedEvent_<editor::dialog_request_close_event_args>(this, editor::dialog_content_base::request_close_event);
}

ff::co_task_source<int> editor::dialog_content_base::awaitable() const
{
    return this->task;
}

void editor::dialog_content_base::dialog_opened()
{}

void editor::dialog_content_base::dialog_closed(int result)
{
    this->task.set_result(result);
}

bool editor::dialog_content_base::can_window_close()
{
    return true;
}

bool editor::dialog_content_base::has_close_command(int result)
{
    return result != editor::dialog_content_base::RESULT_NO;
}

ff::co_task<bool> editor::dialog_content_base::apply_changes_async(int result)
{
    co_return true;
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
    Noesis::Ptr<editor::dialog_content_base> this_ptr(this);
    int result = ::get_result(param);

    auto allow_close_func = [this_ptr, result]() -> ff::co_task<bool>
    {
        bool allow_close = (result == editor::dialog_content_base::RESULT_CANCEL);

        if (!allow_close)
        {
            allow_close = co_await this_ptr->apply_changes_async(result);
        }

        co_return allow_close;
    };

    allow_close_func().continue_with<void>([this_ptr, result](ff::co_task<bool> allow_close_task)
    {
        if (allow_close_task.result())
        {
            editor::dialog_request_close_event_args args(result, this_ptr, editor::dialog_content_base::request_close_event);
            this_ptr->RaiseEvent(args);
            assert(args.handled);
        }
    });
}

bool editor::dialog_content_base::close_command_enabled(Noesis::BaseComponent* param)
{
    return this->has_close_command(::get_result(param));
}
