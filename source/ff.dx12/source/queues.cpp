#include "pch.h"
#include "globals.h"
#include "queues.h"

ff::dx12::queues::queues()
    : direct_queue(D3D12_COMMAND_LIST_TYPE_DIRECT)
    , compute_queue(D3D12_COMMAND_LIST_TYPE_COMPUTE)
    , copy_queue(D3D12_COMMAND_LIST_TYPE_COPY)
{}

ff::dx12::queue& ff::dx12::queues::direct()
{
    return this->direct_queue;
}

ff::dx12::queue& ff::dx12::queues::compute()
{
    return this->compute_queue;
}

ff::dx12::queue& ff::dx12::queues::copy()
{
    return this->copy_queue;
}

ff::dx12::queue& ff::dx12::queues::from_type(D3D12_COMMAND_LIST_TYPE type)
{
    switch (type)
    {
        default:
            return this->direct_queue;

        case D3D12_COMMAND_LIST_TYPE_COPY:
            return this->copy_queue;

        case D3D12_COMMAND_LIST_TYPE_COMPUTE:
            return this->compute_queue;
    }
}

void ff::dx12::queues::wait_for_idle()
{
    this->copy_queue.wait_for_idle();
    this->compute_queue.wait_for_idle();
    this->direct_queue.wait_for_idle();
}
