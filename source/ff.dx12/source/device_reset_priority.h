#pragma once

namespace ff::dx12
{
    enum class device_reset_priority
    {
        fence,
        heap,
        cpu_descriptor_allocator,
        gpu_descriptor_allocator,
        resource,
        queue,
        commands,
        normal,
        target_window,
    };
}
