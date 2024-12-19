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
    ff::gamepad_device& gamepad(size_t index);
    size_t gamepad_count();
}

namespace ff::internal::input
{
    bool init();
    void destroy();

    void add_device(ff::input_device_base* device);
    void remove_device(ff::input_device_base* device);
}
