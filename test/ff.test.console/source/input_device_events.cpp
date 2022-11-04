#include "pch.h"

void run_input_device_events()
{
    ff::init_main_window init_main_window(ff::init_main_window_params{ {}, "Test window", true });
    ff::init_input init_input;

    std::mutex mutex;
    ff::win_handle done_event = ff::win_handle::create_event();
    ff::input_device_event last_event{};

    auto connection = ff::input::combined_devices().event_sink().connect([&mutex, &last_event](const ff::input_device_event& event)
        {
            std::scoped_lock lock(mutex);
            bool ignore = (event.type == ff::input_device_event_type::mouse_move ||
                event.type == ff::input_device_event_type::touch_move) &&
                event.type == last_event.type;

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
                std::cout << str.str();
            }

            last_event = event;
        });

    ff::win_handle thread_handle = ff::thread_pool::get()->add_thread([&done_event]()
        {
            ff::thread_dispatch dispatch(ff::thread_dispatch_type::game);

            while (!ff::wait_for_event_and_reset(done_event, 20))
            {
                ff::input::combined_devices().advance();
                dispatch.flush();
            }
        });

    ff::handle_messages_until_quit();
    ::SetEvent(done_event);
    ff::wait_for_handle(thread_handle);
}
