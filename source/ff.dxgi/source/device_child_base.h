#pragma once

namespace ff::dxgi
{
    class device_child_base
    {
    public:
        virtual ~device_child_base() = default;

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

    private:
        ff::signal<device_child_base*, bool&> after_reset_signal;
    };
}
