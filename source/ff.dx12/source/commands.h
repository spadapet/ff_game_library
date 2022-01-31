#pragma once

#include "access.h"
#include "fence.h"
#include "fence_values.h"
#include "resource_tracker.h"

namespace ff::dx12
{
    class buffer;
    class buffer_base;
    class depth;
    class descriptor_range;
    class mem_range;
    class residency_access;
    class residency_data;
    class resource;
    class resource_tracker;
    class queue;
    enum class gpu_event;

    /// <summary>
    /// Wrapper for DX12 command lists, with automatic resource management (states and residency)
    /// </summary>
    class commands : public ff::dxgi::command_context_base
    {
    public:
        struct data_cache_t
        {
            data_cache_t(ff::dx12::queue* queue);
            data_cache_t(data_cache_t&& other) noexcept = delete;
            data_cache_t(const data_cache_t& other) = delete;

            data_cache_t& operator=(data_cache_t&& other) noexcept = delete;
            data_cache_t& operator=(const data_cache_t& other) = delete;

            Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList1> list;
            Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> list_before;
            Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
            Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator_before;
            std::unordered_set<ff::dx12::residency_data*> residency_set;
            ff::dx12::resource_tracker resource_tracker;
            ff::dx12::fence fence;
            ff::win_handle lists_reset_event;
        };

        commands(ff::dx12::queue& queue, std::unique_ptr<ff::dx12::commands::data_cache_t>&& data_cache);
        commands(commands&& other) noexcept = default;
        commands(const commands& other) = delete;
        ~commands();

        static commands& get(ff::dxgi::command_context_base& obj);
        commands& operator=(commands&& other) noexcept = default;
        commands& operator=(const commands& other) = delete;

        operator bool() const;
        ff::dx12::queue& queue() const;
        ff::dx12::fence_value next_fence_value();

        // PIX events
        void begin_event(ff::dx12::gpu_event type);
        void end_event();

        // For queue use only
        void close_command_lists(ff::dx12::commands* prev_commands, ff::dx12::commands* next_commands, ff::dx12::fence_values& wait_before_execute);
        std::unique_ptr<ff::dx12::commands::data_cache_t> take_data();

        void pipeline_state(ID3D12PipelineState* state);
        void resource_state(ff::dx12::resource& resource, D3D12_RESOURCE_STATES state, size_t array_start = 0, size_t array_size = 0, size_t mip_start = 0, size_t mip_size = 0);
        void resource_state_sub_index(ff::dx12::resource& resource, D3D12_RESOURCE_STATES state, size_t sub_index);
        void resource_alias(ff::dx12::resource* resource_before, ff::dx12::resource* resource_after);

        void root_signature(ID3D12RootSignature* signature);
        void root_descriptors(size_t index, ff::dx12::descriptor_range& range, size_t base_index = 0);
        void root_constant(size_t index, uint32_t data, size_t data_index = 0);
        void root_constants(size_t index, const void* data, size_t size, size_t data_index = 0);
        void root_cbv(size_t index, ff::dx12::buffer& buffer, size_t offset = 0);
        void root_cbv(size_t index, ff::dx12::resource& resource, D3D12_GPU_VIRTUAL_ADDRESS data);
        void root_srv(size_t index, ff::dx12::resource& resource, D3D12_GPU_VIRTUAL_ADDRESS data, bool ps_access = true, bool non_ps_access = true);
        void root_uav(size_t index, ff::dx12::resource& resource, D3D12_GPU_VIRTUAL_ADDRESS data);

        void targets(ff::dxgi::target_base** targets, size_t count, ff::dxgi::depth_base* depth);
        void viewports(const D3D12_VIEWPORT* viewports, size_t count);
        void vertex_buffers(ff::dx12::buffer_base** buffers, const D3D12_VERTEX_BUFFER_VIEW* views, size_t start, size_t count);
        void index_buffer(ff::dx12::buffer_base& buffer, const D3D12_INDEX_BUFFER_VIEW& view);
        void primitive_topology(D3D12_PRIMITIVE_TOPOLOGY topology);
        void stencil(uint32_t value);
        void draw(size_t start, size_t count);
        void draw(size_t start_index, size_t index_count, size_t start_vertex);
        void resolve(ff::dx12::resource& dest_resource, size_t dest_sub_resource, ff::point_size dest_pos, ff::dx12::resource& src_resource, size_t src_sub_resource, ff::rect_size src_rect);

        void clear(const ff::dx12::depth& depth, const float* depth_value, const BYTE* stencil_value);
        void discard(const ff::dx12::depth& depth);

        void clear(ff::dxgi::target_base& target, const DirectX::XMFLOAT4& color);
        void discard(ff::dxgi::target_base& target);

        void update_buffer(ff::dx12::resource& dest, uint64_t dest_offset, ff::dx12::mem_range& source);
        void readback_buffer(ff::dx12::mem_range& dest, ff::dx12::resource& source, uint64_t source_offset);

        void update_texture(ff::dx12::resource& dest, size_t dest_sub_index, ff::point_size dest_pos, ff::dx12::mem_range& source, const D3D12_SUBRESOURCE_FOOTPRINT& source_layout);
        void readback_texture(ff::dx12::mem_range& dest, const D3D12_SUBRESOURCE_FOOTPRINT& dest_layout, ff::dx12::resource& source, size_t source_sub_index, ff::rect_size source_rect);

        void copy_resource(ff::dx12::resource& dest_resource, ff::dx12::resource& source_resource);
        void copy_texture(ff::dx12::resource& dest, size_t dest_sub_index, ff::point_size dest_pos, ff::dx12::resource& source, size_t source_sub_index, ff::rect_size source_rect);

    private:
        ID3D12GraphicsCommandList1* list(bool flush_resource_state = true) const;
        ff::dx12::resource_tracker* tracker() const;
        void keep_resident(ff::dx12::residency_access& access);

        D3D12_COMMAND_LIST_TYPE type_;
        ff::dx12::queue* queue_;
        std::unique_ptr<ff::dx12::commands::data_cache_t> data_cache;
        ff::dx12::fence_values wait_before_execute;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_state_;
        Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_;
    };
}
