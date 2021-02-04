#pragma once

namespace ff
{
    /// <summary>
    /// Access the state of any virtual key
    /// </summary>
    class input_vk
    {
    public:
        virtual ~input_vk() = default;

        virtual bool pressing(int vk) const = 0;
        virtual int press_count(int vk) const = 0;
        virtual float analog_value(int vk) const;
    };
}
