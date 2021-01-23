#pragma once

namespace ff
{
    class input_vk
    {
    public:
        virtual ~input_vk() = 0;

        virtual bool pressing(int vk) const = 0;
        virtual int press_count(int vk) const = 0;
        virtual float analog_value(int vk) const;
    };
}
