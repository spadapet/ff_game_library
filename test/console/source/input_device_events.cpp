#include "pch.h"

void run_input_device_events()
{
    ff::init_input init_input;

    auto connection = ff::input::combined_devices().event_sink().connect([](const ff::input_device_event& event)
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
            std::cout << str.str();
        });

    ff::handle_messages_until_quit();
}
