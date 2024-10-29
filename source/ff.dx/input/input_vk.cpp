#include "pch.h"
#include "input/input_vk.h"

float ff::input_vk::analog_value(int vk) const
{
    return this->pressing(vk) ? 1.0f : 0.0f;
}
