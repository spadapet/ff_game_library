#pragma once

namespace ff::internal::dx11
{
    enum class device_reset_priority
    {
        normal,
        target_window,
    };

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
    };
}
