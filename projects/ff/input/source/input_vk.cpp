#include "pch.h"
#include "input_vk.h"

ff::input_vk::~input_vk()
{}

float ff::input_vk::analog_value(int vk) const
{
    return this->pressing(vk) ? 1.0f : 0.0f;
}
