#include "pch.h"
#include "input/input_device_event.h"

ff::input_device_event::input_device_event()
    : input_device_event(input_device_event_type::none)
{}

ff::input_device_event::input_device_event(input_device_event_type type, unsigned int id, int count, ff::point_int pos)
    : type(type)
    , id(id)
    , count(count)
    , pos(pos)
{}

ff::input_device_event_key_press::input_device_event_key_press(unsigned int vk, int count)
    : input_device_event(ff::input_device_event_type::key_press, vk, count)
{}

ff::input_device_event_key_char::input_device_event_key_char(unsigned int ch)
    : input_device_event(ff::input_device_event_type::key_char, ch)
{}

ff::input_device_event_mouse_press::input_device_event_mouse_press(unsigned int vk, int count, ff::point_int pos)
    : input_device_event(ff::input_device_event_type::mouse_press, vk, count, pos)
{}

ff::input_device_event_mouse_move::input_device_event_mouse_move(ff::point_int pos)
    : input_device_event(ff::input_device_event_type::mouse_move, 0, 0, pos)
{}

ff::input_device_event_mouse_wheel_x::input_device_event_mouse_wheel_x(int scroll, ff::point_int pos)
    : input_device_event(ff::input_device_event_type::mouse_wheel_x, 0, scroll, pos)
{}

ff::input_device_event_mouse_wheel_y::input_device_event_mouse_wheel_y(int scroll, ff::point_int pos)
    : input_device_event(ff::input_device_event_type::mouse_wheel_y, 0, scroll, pos)
{}

ff::input_device_event_touch_press::input_device_event_touch_press(unsigned int id, int count, ff::point_int pos)
    : input_device_event(ff::input_device_event_type::touch_press, id, count, pos)
{}

ff::input_device_event_touch_move::input_device_event_touch_move(unsigned int id, ff::point_int pos)
    : input_device_event(ff::input_device_event_type::touch_move, id, 0, pos)
{}
