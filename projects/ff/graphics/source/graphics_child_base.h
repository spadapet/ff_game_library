#pragma once

namespace ff::internal
{
    namespace graphics_reset_priorities
    {
        static const int dx12_cpu_descriptor_allocator = 100;
        static const int dx12_gpu_descriptor_allocator = 99;
        static const int dx12_frame_mem_allocator = 98;
        static const int dx12_command_queue = 97;
        static const int dx12_commands = 96;
        static const int normal = 0;
        static const int target_window = -100;
    };

    class graphics_child_base
    {
    public:
        virtual ~graphics_child_base() = default;

        virtual bool reset() = 0;
        virtual int reset_priority() const;
    };
}
