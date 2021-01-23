#pragma once

namespace ff
{
    class input_vk;

    struct input_event_def
    {
        size_t event_id;
        double hold_seconds;
        double repeat_seconds;
        std::array<int, 4> vk;
    };

    struct input_value_def
    {
        size_t value_id;
        int vk;
    };

    struct input_event
    {
        size_t event_id;
        size_t count;

        bool started() const { return this->count == 1; }
        bool repeated() const { return this->count > 1; }
        bool stopped() const { return this->count == 0; }
    };

    struct input_devices
    {
        std::vector<input_vk const*> devices;
    };
}
