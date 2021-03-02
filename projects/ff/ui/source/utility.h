#pragma once

namespace ff::internal::ui
{
    bool valid_key(unsigned int vk);
    Noesis::Key get_key(unsigned int vk);
    bool valid_mouse_button(unsigned int vk);
    Noesis::MouseButton get_mouse_button(unsigned int vk);
}
