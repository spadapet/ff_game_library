#pragma once

#include "access.h"
#include "fence_values.h"

namespace ff::dx12
{
    class buffer;
    class depth;
    class mem_range;
    class resource;
    class resource_tracker;
    class queue;

    class commands : public ff::dxgi::command_context_base, private ff::dxgi::device_child_base
    {
    public:
        struct data_cache_t
        {
            Microsoft::WRL::ComPtr<ID3D12GraphicsCommandListX> list;
            Microsoft::WRL::ComPtr<ID3D12GraphicsCommandListX> list_before;
            Microsoft::WRL::ComPtr<ID3D12CommandAllocatorX> allocator;
            std::unique_ptr<ff::dx12::resource_tracker> resource_tracker;
            std::unique_ptr<ff::dx12::fence> fence;
        };

        commands(ff::dx12::queue& queue, ff::dx12::commands::data_cache_t&& data_cache, ID3D12PipelineStateX* initial_state);
        commands(commands&& other) noexcept;
        commands(const commands& other) = delete;
        virtual ~commands() override;

        static commands& get(ff::dxgi::command_context_base& obj);
        commands& operator=(commands&& other) noexcept = default;
        commands& operator=(const commands& other) = delete;

        operator bool() const;
        ff::dx12::queue& queue() const;
        ff::dx12::fence_value next_fence_value();
        void flush(ff::dx12::commands* prev_commands, ff::dx12::commands* next_commands, ff::dx12::fence_values& wait_before_execute);
        ff::dx12::commands::data_cache_t close();

        void pipeline_state(ID3D12PipelineStateX* state);
        void resource_state(ff::dx12::resource& resource, D3D12_RESOURCE_STATES state);
        void resource_state(ff::dx12::resource& resource, D3D12_RESOURCE_STATES state, size_t sub_index);

        void root_signature(ID3D12RootSignature* signature);
        void root_descriptors(size_t index, D3D12_GPU_DESCRIPTOR_HANDLE base_descriptor);
        void root_constant(size_t index, uint32_t data, size_t data_index = 0);
        void root_constants(size_t index, const void* data, size_t size, size_t data_index = 0);
        void root_cbv(size_t index, ff::dx12::buffer& buffer, size_t offset = 0);
        void root_cbv(size_t index, ff::dx12::resource& resource, D3D12_GPU_VIRTUAL_ADDRESS data);
        void root_srv(size_t index, ff::dx12::resource& resource, D3D12_GPU_VIRTUAL_ADDRESS data, bool ps_access = true, bool non_ps_access = true);
        void root_uav(size_t index, ff::dx12::resource& resource, D3D12_GPU_VIRTUAL_ADDRESS data);

        void targets(ff::dxgi::target_base* targets, size_t count, ff::dxgi::depth_base* depth);
        void viewports(const D3D12_VIEWPORT* viewports, size_t count);
        void vertex_buffers(ff::dx12::resource* resources, const D3D12_VERTEX_BUFFER_VIEW* views, size_t start, size_t count);
        void primitive_topology(D3D12_PRIMITIVE_TOPOLOGY topology);
        void draw(size_t start, size_t count);

        void clear(const ff::dx12::depth& depth, const float* depth_value, const BYTE* stencil_value);
        void discard(const ff::dx12::depth& depth);

        void clear(ff::dxgi::target_base& target, const DirectX::XMFLOAT4& color);
        void discard(ff::dxgi::target_base& target);

        void update_buffer(ff::dx12::resource& dest, uint64_t dest_offset, const ff::dx12::mem_range& source);
        void readback_buffer(const ff::dx12::mem_range& dest, ff::dx12::resource& source, uint64_t source_offset);

        void update_texture(ff::dx12::resource& dest, size_t dest_sub_index, ff::point_size dest_pos, const ff::dx12::mem_range& source, const D3D12_SUBRESOURCE_FOOTPRINT& source_layout);
        void readback_texture(const ff::dx12::mem_range& dest, const D3D12_SUBRESOURCE_FOOTPRINT& dest_layout, ff::dx12::resource& source, size_t source_sub_index, ff::rect_size source_rect);

        void copy_resource(ff::dx12::resource& dest_resource, ff::dx12::resource& source_resource);
        void copy_texture(ff::dx12::resource& dest, size_t dest_sub_index, ff::point_size dest_pos, ff::dx12::resource& source, size_t source_sub_index, ff::rect_size source_rect);

    private:
        ID3D12GraphicsCommandListX* list(bool flush_resource_state = true) const;
        ff::dx12::resource_tracker* tracker() const;

        // device_child_base
        virtual void before_reset() override;

        D3D12_COMMAND_LIST_TYPE type_;
        ff::dx12::queue* queue_;
        ff::dx12::commands::data_cache_t data_cache;
        ff::dx12::fence_values wait_before_execute;
        Microsoft::WRL::ComPtr<ID3D12PipelineStateX> pipeline_state_;
        Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_;
    };
}
