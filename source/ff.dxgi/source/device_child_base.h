#pragma once

namespace ff::dxgi
{
    class device_child_base
    {
    public:
        device_child_base() = default;
        device_child_base(device_child_base&& other) noexcept;
        device_child_base(const device_child_base& other) = delete;
        virtual ~device_child_base() = default;

        device_child_base& operator=(device_child_base&& other) noexcept;
        device_child_base& operator=(const device_child_base& other) = delete;

        // In before_reset(), release any GPU objects and optionally use the ff::frame_allocator to remember how to recreate them during reset()
        virtual void before_reset();
        virtual void* before_reset(ff::frame_allocator& allocator);

        // Recreate any GPU objects
        virtual bool reset();
        virtual bool reset(void* data);

        // Called after every GPU object has been recreated, now you can upload new GPU data
        virtual bool after_reset();
        bool call_after_reset();
        ff::signal_sink<device_child_base*, bool&>& after_reset_sink();

        ff::dxgi::device_child_base* next_device_child_{};
        ff::dxgi::device_child_base* prev_device_child_{};
        int device_child_reset_priority_{};

    private:
        ff::signal<device_child_base*, bool&> after_reset_signal;
    };
}
