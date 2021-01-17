#pragma once

namespace ff
{
    class controller_device;
    class input_device_base;
    class keyboard_device;
    class pointer_device;
}

namespace ff::input
{
    ff::input_device_base& combined_devices();
    ff::keyboard_device& keyboard();
    ff::pointer_device& pointer();
    ff::controller_device& controller(size_t index);
    size_t controller_count();
}

namespace ff::input::internal
{
    bool init();
    void destroy();

    void add_device(ff::input_device_base* device);
    void remove_device(ff::input_device_base* device);
}
