#pragma once

#include "queue.h"

namespace ff::dx12
{
    class queues
    {
    public:
        queues();
        queues(queues&& other) noexcept = delete;
        queues(const queues& other) = delete;
    
        queues& operator=(queues&& other) noexcept = delete;
        queues& operator=(const queues& other) = delete;

        ff::dx12::queue& direct();
        ff::dx12::queue& compute();
        ff::dx12::queue& copy();
        ff::dx12::queue& from_type(D3D12_COMMAND_LIST_TYPE type);
        void wait_for_idle();

    private:
        ff::dx12::queue direct_queue;
        ff::dx12::queue compute_queue;
        ff::dx12::queue copy_queue;
    };
}
