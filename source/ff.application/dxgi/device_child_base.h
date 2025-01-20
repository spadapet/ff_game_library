#pragma once

#include "../dx_types/intrusive_list.h"

namespace ff::dxgi
{
    class device_child_base : public ff::intrusive_list::data<device_child_base>
    {
    public:
        device_child_base() = default;
        device_child_base(device_child_base&& other) noexcept = default;
        device_child_base(const device_child_base& other) = delete;
        virtual ~device_child_base() = default;

        device_child_base& operator=(device_child_base&& other) noexcept = default;
        device_child_base& operator=(const device_child_base& other) = delete;

        // In before_reset(), release any GPU objects and optionally use the ff::frame_allocator to remember how to recreate them during reset()
        virtual void before_reset();
        virtual void* before_reset(ff::frame_allocator& allocator);

        // Recreate any GPU objects
        virtual bool reset();
        virtual bool reset(void* data);

        // Called after every GPU object has been recreated
        virtual bool after_reset();

        int device_child_reset_priority_{};
    };
}
