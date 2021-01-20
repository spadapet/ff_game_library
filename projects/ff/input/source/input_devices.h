#pragma once

namespace ff
{
    enum class input_device : unsigned char
    {
        // NOTE: These are persisted, so don't change their order/values

        none,
        keyboard,
        mouse,
        joystick,
        touch,
        touchpad,
        pen,
        pointer, // generic
    };

    // What is the user touching on the device?
    enum class input_part : unsigned char
    {
        // NOTE: These are persisted, so don't change their order/values

        none,
        button, // keyboard, mouse, or joystick
        text, // on keyboard
        key_button, // joystick button using vk code
        stick, // joystick
        dpad, // joystick
        trigger, // joystick
    };

    enum class input_part_value : unsigned char
    {
        // NOTE: These are persisted, so don't change their order/values

        default,
        x_axis, // joystick
        y_axis, // joystick
        left, // joystick
        right, // joystick
        up, // joystick
        down, // joystick
    };
}
