#pragma once

namespace ff::dx12
{
    class queue;

    /// <summary>
    /// Owns the DX12 queues. This should not be shared across threads.
    /// </summary>
    class queues
    {
    public:
        queues() = default;
        queues(queues&& other) noexcept = default;
        queues(const queues& other) = delete;
    
        queues& operator=(queues&& other) noexcept = default;
        queues& operator=(const queues& other) = delete;

        ff::dx12::queue& direct();
        ff::dx12::queue& compute();
        ff::dx12::queue& copy();
        ff::dx12::queue& from_type(D3D12_COMMAND_LIST_TYPE type);
        void wait_for_idle();

    private:
        std::unique_ptr<ff::dx12::queue> direct_queue;
        std::unique_ptr<ff::dx12::queue> compute_queue;
        std::unique_ptr<ff::dx12::queue> copy_queue;
    };
}
