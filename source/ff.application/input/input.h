#pragma once

namespace ff
{
    class gamepad_device;
    class input_device_base;
    class keyboard_device;
    class pointer_device;
}

namespace ff::input
{
    ff::input_device_base& combined_devices();
    ff::keyboard_device& keyboard();
    ff::pointer_device& pointer();
    ff::gamepad_device& gamepad(); // return first connected
    ff::gamepad_device& gamepad(size_t index);
    size_t gamepad_count();

    ff::input_device_base& debug_devices();
    ff::input_device_base& keyboard_debug();
    ff::input_device_base& pointer_debug();
}

namespace ff::internal::input
{
    bool init();
    void destroy();
    bool app_window_active();
}
