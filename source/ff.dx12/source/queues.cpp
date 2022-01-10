#include "pch.h"
#include "queue.h"
#include "queues.h"

ff::dx12::queue& ff::dx12::queues::direct()
{
    if (!this->direct_queue)
    {
        this->direct_queue = std::make_unique<ff::dx12::queue>("Direct queue", D3D12_COMMAND_LIST_TYPE_DIRECT);
    }

    return *this->direct_queue;
}

ff::dx12::queue& ff::dx12::queues::compute()
{
    if (!this->compute_queue)
    {
        this->compute_queue = std::make_unique<ff::dx12::queue>("Compute queue", D3D12_COMMAND_LIST_TYPE_COMPUTE);
    }

    return *this->compute_queue;
}

ff::dx12::queue& ff::dx12::queues::copy()
{
    if (!this->copy_queue)
    {
        this->copy_queue = std::make_unique<ff::dx12::queue>("Copy queue", D3D12_COMMAND_LIST_TYPE_COPY);
    }

    return *this->copy_queue;
}

ff::dx12::queue& ff::dx12::queues::from_type(D3D12_COMMAND_LIST_TYPE type)
{
    switch (type)
    {
        default:
            return this->direct();

        case D3D12_COMMAND_LIST_TYPE_COPY:
            return this->copy();

        case D3D12_COMMAND_LIST_TYPE_COMPUTE:
            return this->compute();
    }
}

void ff::dx12::queues::wait_for_idle()
{
    if (this->copy_queue)
    {
        this->copy_queue->wait_for_idle();
    }

    if (this->compute_queue)
    {
        this->compute_queue->wait_for_idle();
    }

    if (this->direct_queue)
    {
        this->direct_queue->wait_for_idle();
    }
}
