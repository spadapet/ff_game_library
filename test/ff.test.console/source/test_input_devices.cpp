#include "pch.h"

using namespace std::literals::chrono_literals;

void run_test_input_devices()
{
    ff::init_main_window_params window_params{ {}, "Test window", true };
    ff::init_base init_main_window(&window_params);
    ff::init_input init_input;

    std::mutex mutex;
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

    std::jthread thread([](std::stop_token stop_token)
        {
            ff::thread_dispatch dispatch(ff::thread_dispatch_type::game);

            while (!stop_token.stop_requested())
            {
                ff::input::combined_devices().advance();
                std::this_thread::sleep_for(20ms);
                dispatch.flush();
            }
        });

    ff::handle_messages_until_quit();
    thread.get_stop_source().request_stop();
}
