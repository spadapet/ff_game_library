#include "pch.h"
#include "test_input_devices.xaml.h"

test_uwp::test_input_devices::test_input_devices()
{
    this->InitializeComponent();
}

void test_uwp::test_input_devices::loaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args)
{
    this->input_connection = ff::input::combined_devices().event_sink().connect([this](const ff::input_device_event& event)
        {
            assert(event.type != ff::input_device_event_type::none);

            this->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal,
                ref new Windows::UI::Core::DispatchedHandler([this, event]()
                {
                    this->handle_input_event(event);
                }));
        });
}

void test_uwp::test_input_devices::unloaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args)
{
    this->input_connection.disconnect();
}

void test_uwp::test_input_devices::handle_input_event(const ff::input_device_event& event)
{
    bool ignore = (event.type == ff::input_device_event_type::mouse_move ||
        event.type == ff::input_device_event_type::touch_move) &&
        event.type == this->last_event.type;

    if (!ignore)
    {
        std::string_view name = "<invalid>";

        switch (event.type)
        {
            case ff::input_device_event_type::key_char: name = "key_char"; break;
            case ff::input_device_event_type::key_press: name = "key_press"; break;
            case ff::input_device_event_type::mouse_move: name = "mouse_move"; break;
            case ff::input_device_event_type::mouse_press: name = "mouse_press"; break;
            case ff::input_device_event_type::mouse_wheel_x: name = "mouse_wheel_x"; break;
            case ff::input_device_event_type::mouse_wheel_y: name = "mouse_wheel_y"; break;
            case ff::input_device_event_type::touch_move: name = "touch_move"; break;
            case ff::input_device_event_type::touch_press: name = "touch_press"; break;
            default: assert(false); break;
        }

        std::ostringstream str;
        str << name << ": id=" << event.id << ", pos=(" << event.pos.x << "," << event.pos.y << "), count=" << event.count << std::endl;
        auto paragraph = ref new Windows::UI::Xaml::Documents::Paragraph();
        auto run = ref new Windows::UI::Xaml::Documents::Run();
        run->Text = ff::string::to_pstring(str.str());
        paragraph->Inlines->Append(run);
        this->output_box->Blocks->Append(paragraph);

        this->scroller->UpdateLayout();
        this->scroller->ChangeView(0.0, this->scroller->ScrollableHeight, 1.0f, true);
    }

    this->last_event = event;
}
