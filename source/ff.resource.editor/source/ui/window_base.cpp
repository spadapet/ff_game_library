#include "pch.h"
#include "source/ui/window_base.h"

const Noesis::DependencyProperty* editor::window_base::title_property{};

static void title_changed(Noesis::DependencyObject* object, const Noesis::DependencyPropertyChangedEventArgs& args)
{
    const Noesis::String& value = args.NewValue<Noesis::String>();
    std::wstring title = ff::string::to_wstring(std::string_view(value.Str(), value.Size()));

    ff::thread_dispatch::get_main()->post([title = std::move(title)]()
    {
        ::SetWindowText(*ff::window::main(), title.c_str());
    });
}

editor::window_base::window_base()
    : message_connection(ff::window::main()->message_sink().connect(std::bind(&editor::window_base::handle_message, this, std::placeholders::_1)))
{
    this->Loaded() += Noesis::MakeDelegate(this, &editor::window_base::on_loaded);
}

editor::window_base::~window_base()
{
    this->Loaded() -= Noesis::MakeDelegate(this, &editor::window_base::on_loaded);
}

void editor::window_base::handle_message(ff::window_message& message)
{
    switch (message.msg)
    {
        case WM_ENABLE:
            ff::thread_dispatch::get_game()->send([this, &message]()
            {
                this->SetIsEnabled(message.wp != 0);
            });
            break;

        case WM_CLOSE:
            ff::thread_dispatch::get_game()->send([this, &message]()
            {
                if (!this->can_close())
                {
                    message.result = 0;
                    message.handled = true;
                }
            });
            break;

        case editor::window_base::WM_USER_FORCE_CLOSE:
            ::DestroyWindow(message.hwnd);
            break;
    }
}

bool editor::window_base::can_close()
{
    return true;
}

void editor::window_base::on_loaded(Noesis::BaseComponent*, const Noesis::RoutedEventArgs&)
{
    this->SetIsEnabled(::IsWindowEnabled(*ff::window::main()) != FALSE);
}

NS_IMPLEMENT_REFLECTION(editor::window_base, "Window")
{
    Noesis::DependencyData* data = NsMeta<Noesis::DependencyData>(Noesis::TypeOf<SelfClass>());
    data->RegisterProperty<Noesis::String>(editor::window_base::title_property, "Title", Noesis::PropertyMetadata::Create(Noesis::String(), ::title_changed));
}
