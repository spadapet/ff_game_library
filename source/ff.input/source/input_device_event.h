#pragma once

namespace ff
{
    enum class input_device
    {
        none,
        keyboard,
        mouse,
        gamepad,
        touch,
        touchpad,
        pen,
        pointer, // generic
    };

    enum class input_device_event_type
    {
        none,
        key_press,
        key_char,
        mouse_press,
        mouse_move,
        mouse_wheel_x,
        mouse_wheel_y,
        touch_press,
        touch_move,
    };

    struct input_device_event
    {
        input_device_event();
        input_device_event(input_device_event_type type, unsigned int id = 0, int count = 0, ff::point_int pos = {});

        input_device_event_type type;
        unsigned int id;
        int count;
        ff::point_int pos;
    };

    struct input_device_event_key_press : public input_device_event
    {
        input_device_event_key_press(unsigned int vk, int count);
    };

    struct input_device_event_key_char : public input_device_event
    {
        input_device_event_key_char(unsigned int ch);
    };

    struct input_device_event_mouse_press : public input_device_event
    {
        input_device_event_mouse_press(unsigned int vk, int count, ff::point_int pos);
    };

    struct input_device_event_mouse_move : public input_device_event
    {
        input_device_event_mouse_move(ff::point_int pos);
    };

    struct input_device_event_mouse_wheel_x : public input_device_event
    {
        input_device_event_mouse_wheel_x(int scroll, ff::point_int pos);
    };

    struct input_device_event_mouse_wheel_y : public input_device_event
    {
        input_device_event_mouse_wheel_y(int scroll, ff::point_int pos);
    };

    struct input_device_event_touch_press : public input_device_event
    {
        input_device_event_touch_press(unsigned int id, int count, ff::point_int pos);
    };

    struct input_device_event_touch_move : public input_device_event
    {
        input_device_event_touch_move(unsigned int id, ff::point_int pos);
    };
}
