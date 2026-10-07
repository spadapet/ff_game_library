#include "pch.h"

namespace ff::test::dx12
{
    static D3D12_RESOURCE_DESC tracker_texture_desc(UINT16 array_size = 1, UINT16 mip_levels = 1)
    {
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = 64;
        desc.Height = 64;
        desc.DepthOrArraySize = array_size;
        desc.MipLevels = mip_levels;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        return desc;
    }

    class tracker_barrier_recorder final : public ID3D12GraphicsCommandList
    {
    public:
        D3D12_RESOURCE_BARRIER barriers[32]{};
        size_t count{};

        void STDMETHODCALLTYPE ResourceBarrier(UINT barrier_count, const D3D12_RESOURCE_BARRIER* values) override
        {
            Assert::IsTrue(this->count + barrier_count <= _countof(this->barriers));
            for (UINT i = 0; i < barrier_count; i++)
            {
                this->barriers[this->count++] = values[i];
            }
        }

        D3D12_COMMAND_LIST_TYPE STDMETHODCALLTYPE GetType() override { return D3D12_COMMAND_LIST_TYPE_DIRECT; }
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override { return E_NOINTERFACE; }
        ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
        ULONG STDMETHODCALLTYPE Release() override { return 1; }
        HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID, UINT*, void*) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID, UINT, const void*) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID, const IUnknown*) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE SetName(LPCWSTR) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE GetDevice(REFIID, void**) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE Close() override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE Reset(ID3D12CommandAllocator*, ID3D12PipelineState*) override { return E_NOTIMPL; }

#define TRACKER_UNEXPECTED_COMMAND(name, ...) \
        void STDMETHODCALLTYPE name(__VA_ARGS__) override { Assert::Fail(L"Unexpected command list call"); }

        TRACKER_UNEXPECTED_COMMAND(ClearState, ID3D12PipelineState*)
        TRACKER_UNEXPECTED_COMMAND(DrawInstanced, UINT, UINT, UINT, UINT)
        TRACKER_UNEXPECTED_COMMAND(DrawIndexedInstanced, UINT, UINT, UINT, INT, UINT)
        TRACKER_UNEXPECTED_COMMAND(Dispatch, UINT, UINT, UINT)
        TRACKER_UNEXPECTED_COMMAND(CopyBufferRegion, ID3D12Resource*, UINT64, ID3D12Resource*, UINT64, UINT64)
        TRACKER_UNEXPECTED_COMMAND(CopyTextureRegion, const D3D12_TEXTURE_COPY_LOCATION*, UINT, UINT, UINT, const D3D12_TEXTURE_COPY_LOCATION*, const D3D12_BOX*)
        TRACKER_UNEXPECTED_COMMAND(CopyResource, ID3D12Resource*, ID3D12Resource*)
        TRACKER_UNEXPECTED_COMMAND(CopyTiles, ID3D12Resource*, const D3D12_TILED_RESOURCE_COORDINATE*, const D3D12_TILE_REGION_SIZE*, ID3D12Resource*, UINT64, D3D12_TILE_COPY_FLAGS)
        TRACKER_UNEXPECTED_COMMAND(ResolveSubresource, ID3D12Resource*, UINT, ID3D12Resource*, UINT, DXGI_FORMAT)
        TRACKER_UNEXPECTED_COMMAND(IASetPrimitiveTopology, D3D12_PRIMITIVE_TOPOLOGY)
        TRACKER_UNEXPECTED_COMMAND(RSSetViewports, UINT, const D3D12_VIEWPORT*)
        TRACKER_UNEXPECTED_COMMAND(RSSetScissorRects, UINT, const D3D12_RECT*)
        TRACKER_UNEXPECTED_COMMAND(OMSetBlendFactor, const FLOAT*)
        TRACKER_UNEXPECTED_COMMAND(OMSetStencilRef, UINT)
        TRACKER_UNEXPECTED_COMMAND(SetPipelineState, ID3D12PipelineState*)
        TRACKER_UNEXPECTED_COMMAND(ExecuteBundle, ID3D12GraphicsCommandList*)
        TRACKER_UNEXPECTED_COMMAND(SetDescriptorHeaps, UINT, ID3D12DescriptorHeap* const*)
        TRACKER_UNEXPECTED_COMMAND(SetComputeRootSignature, ID3D12RootSignature*)
        TRACKER_UNEXPECTED_COMMAND(SetGraphicsRootSignature, ID3D12RootSignature*)
        TRACKER_UNEXPECTED_COMMAND(SetComputeRootDescriptorTable, UINT, D3D12_GPU_DESCRIPTOR_HANDLE)
        TRACKER_UNEXPECTED_COMMAND(SetGraphicsRootDescriptorTable, UINT, D3D12_GPU_DESCRIPTOR_HANDLE)
        TRACKER_UNEXPECTED_COMMAND(SetComputeRoot32BitConstant, UINT, UINT, UINT)
        TRACKER_UNEXPECTED_COMMAND(SetGraphicsRoot32BitConstant, UINT, UINT, UINT)
        TRACKER_UNEXPECTED_COMMAND(SetComputeRoot32BitConstants, UINT, UINT, const void*, UINT)
        TRACKER_UNEXPECTED_COMMAND(SetGraphicsRoot32BitConstants, UINT, UINT, const void*, UINT)
        TRACKER_UNEXPECTED_COMMAND(SetComputeRootConstantBufferView, UINT, D3D12_GPU_VIRTUAL_ADDRESS)
        TRACKER_UNEXPECTED_COMMAND(SetGraphicsRootConstantBufferView, UINT, D3D12_GPU_VIRTUAL_ADDRESS)
        TRACKER_UNEXPECTED_COMMAND(SetComputeRootShaderResourceView, UINT, D3D12_GPU_VIRTUAL_ADDRESS)
        TRACKER_UNEXPECTED_COMMAND(SetGraphicsRootShaderResourceView, UINT, D3D12_GPU_VIRTUAL_ADDRESS)
        TRACKER_UNEXPECTED_COMMAND(SetComputeRootUnorderedAccessView, UINT, D3D12_GPU_VIRTUAL_ADDRESS)
        TRACKER_UNEXPECTED_COMMAND(SetGraphicsRootUnorderedAccessView, UINT, D3D12_GPU_VIRTUAL_ADDRESS)
        TRACKER_UNEXPECTED_COMMAND(IASetIndexBuffer, const D3D12_INDEX_BUFFER_VIEW*)
        TRACKER_UNEXPECTED_COMMAND(IASetVertexBuffers, UINT, UINT, const D3D12_VERTEX_BUFFER_VIEW*)
        TRACKER_UNEXPECTED_COMMAND(SOSetTargets, UINT, UINT, const D3D12_STREAM_OUTPUT_BUFFER_VIEW*)
        TRACKER_UNEXPECTED_COMMAND(OMSetRenderTargets, UINT, const D3D12_CPU_DESCRIPTOR_HANDLE*, BOOL, const D3D12_CPU_DESCRIPTOR_HANDLE*)
        TRACKER_UNEXPECTED_COMMAND(ClearDepthStencilView, D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_CLEAR_FLAGS, FLOAT, UINT8, UINT, const D3D12_RECT*)
        TRACKER_UNEXPECTED_COMMAND(ClearRenderTargetView, D3D12_CPU_DESCRIPTOR_HANDLE, const FLOAT*, UINT, const D3D12_RECT*)
        TRACKER_UNEXPECTED_COMMAND(ClearUnorderedAccessViewUint, D3D12_GPU_DESCRIPTOR_HANDLE, D3D12_CPU_DESCRIPTOR_HANDLE, ID3D12Resource*, const UINT*, UINT, const D3D12_RECT*)
        TRACKER_UNEXPECTED_COMMAND(ClearUnorderedAccessViewFloat, D3D12_GPU_DESCRIPTOR_HANDLE, D3D12_CPU_DESCRIPTOR_HANDLE, ID3D12Resource*, const FLOAT*, UINT, const D3D12_RECT*)
        TRACKER_UNEXPECTED_COMMAND(DiscardResource, ID3D12Resource*, const D3D12_DISCARD_REGION*)
        TRACKER_UNEXPECTED_COMMAND(BeginQuery, ID3D12QueryHeap*, D3D12_QUERY_TYPE, UINT)
        TRACKER_UNEXPECTED_COMMAND(EndQuery, ID3D12QueryHeap*, D3D12_QUERY_TYPE, UINT)
        TRACKER_UNEXPECTED_COMMAND(ResolveQueryData, ID3D12QueryHeap*, D3D12_QUERY_TYPE, UINT, UINT, ID3D12Resource*, UINT64)
        TRACKER_UNEXPECTED_COMMAND(SetPredication, ID3D12Resource*, UINT64, D3D12_PREDICATION_OP)
        TRACKER_UNEXPECTED_COMMAND(SetMarker, UINT, const void*, UINT)
        TRACKER_UNEXPECTED_COMMAND(BeginEvent, UINT, const void*, UINT)
        TRACKER_UNEXPECTED_COMMAND(EndEvent)
        TRACKER_UNEXPECTED_COMMAND(ExecuteIndirect, ID3D12CommandSignature*, UINT, ID3D12Resource*, UINT64, ID3D12Resource*, UINT64)

#undef TRACKER_UNEXPECTED_COMMAND
    };

    static void assert_tracker_barrier(const D3D12_RESOURCE_BARRIER& barrier, ID3D12Resource* resource,
        D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
    {
        Assert::AreEqual((int)D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, (int)barrier.Type);
        Assert::IsTrue(barrier.Transition.pResource == resource);
        Assert::AreEqual((int)before, (int)barrier.Transition.StateBefore);
        Assert::AreEqual((int)after, (int)barrier.Transition.StateAfter);
        Assert::AreEqual((UINT)D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, barrier.Transition.Subresource);
    }

    TEST_CLASS(dx12_resource_tracker_tests)
    {
    public:
        TEST_METHOD_CLEANUP(cleanup)
        {
            ff_dx12_destroy();
        }

        TEST_METHOD(init_and_destroy_is_clean)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_resource_tracker tracker{};
            ff_dx12_resource_tracker_init(&tracker);
            Assert::AreEqual((size_t)0, ff_array_count(tracker.entries_a));
            ff_dx12_resource_tracker_destroy(&tracker);
        }

        TEST_METHOD(first_transition_is_held_back_as_a_first_barrier)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = tracker_texture_desc();
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("tracked texture"), &desc, nullptr));

            ff_dx12_resource_tracker tracker{};
            ff_dx12_resource_tracker_init(&tracker);

            ff_dx12_resource_tracker_state(&tracker, &resource, D3D12_RESOURCE_STATE_COPY_DEST, 0, 0, 0, 0);

            // Nothing is pending yet: the before-state isn't known until close().
            Assert::AreEqual((size_t)0, ff_array_count(tracker.barriers_pending_a));
            Assert::AreEqual((size_t)1, ff_array_count(tracker.entries_a));
            Assert::AreEqual((size_t)1, ff_array_count(tracker.entries_a[0].first_barriers_a));
            Assert::IsTrue(tracker.entries_a[0].resource == &resource);
            Assert::IsTrue(resource.tracker_references && resource.tracker_references->tracker == &tracker);

            ff_dx12_resource_tracker_reset(&tracker);
            Assert::IsNull((void*)resource.tracker_references);

            ff_dx12_resource_tracker_destroy(&tracker);
            ff_dx12_resource_destroy(&resource);
        }

        TEST_METHOD(second_transition_becomes_a_pending_barrier)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = tracker_texture_desc();
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("two state texture"), &desc, nullptr));

            ff_dx12_resource_tracker tracker{};
            ff_dx12_resource_tracker_init(&tracker);

            ff_dx12_resource_tracker_state(&tracker, &resource, D3D12_RESOURCE_STATE_COPY_DEST, 0, 0, 0, 0);
            ff_dx12_resource_tracker_state(&tracker, &resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, 0, 0, 0);

            Assert::AreEqual((size_t)1, ff_array_count(tracker.barriers_pending_a));
            D3D12_RESOURCE_BARRIER& barrier = tracker.barriers_pending_a[0];
            Assert::AreEqual((int)D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, (int)barrier.Type);
            Assert::AreEqual((int)D3D12_RESOURCE_STATE_COPY_DEST, (int)barrier.Transition.StateBefore);
            Assert::AreEqual((int)D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, (int)barrier.Transition.StateAfter);

            ff_dx12_resource_tracker_reset(&tracker);
            ff_dx12_resource_tracker_destroy(&tracker);
            ff_dx12_resource_destroy(&resource);
        }

        TEST_METHOD(redundant_transition_adds_no_barrier)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = tracker_texture_desc();
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("same state texture"), &desc, nullptr));

            ff_dx12_resource_tracker tracker{};
            ff_dx12_resource_tracker_init(&tracker);

            ff_dx12_resource_tracker_state(&tracker, &resource, D3D12_RESOURCE_STATE_COPY_DEST, 0, 0, 0, 0);
            ff_dx12_resource_tracker_state(&tracker, &resource, D3D12_RESOURCE_STATE_COPY_DEST, 0, 0, 0, 0);

            Assert::AreEqual((size_t)0, ff_array_count(tracker.barriers_pending_a));

            ff_dx12_resource_tracker_reset(&tracker);
            ff_dx12_resource_tracker_destroy(&tracker);
            ff_dx12_resource_destroy(&resource);
        }

        TEST_METHOD(two_resources_get_separate_entries)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = tracker_texture_desc();
            ff_dx12_resource a{};
            ff_dx12_resource b{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&a, FF_SVL("texture a"), &desc, nullptr));
            Assert::IsTrue(ff_dx12_resource_init_committed(&b, FF_SVL("texture b"), &desc, nullptr));

            ff_dx12_resource_tracker tracker{};
            ff_dx12_resource_tracker_init(&tracker);

            ff_dx12_resource_tracker_state(&tracker, &a, D3D12_RESOURCE_STATE_COPY_DEST, 0, 0, 0, 0);
            ff_dx12_resource_tracker_state(&tracker, &b, D3D12_RESOURCE_STATE_COPY_DEST, 0, 0, 0, 0);

            Assert::AreEqual((size_t)2, ff_array_count(tracker.entries_a));

            ff_dx12_resource_tracker_reset(&tracker);
            ff_dx12_resource_tracker_destroy(&tracker);
            ff_dx12_resource_destroy(&a);
            ff_dx12_resource_destroy(&b);
        }

        TEST_METHOD(many_resources_survive_index_map_growth)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            const size_t count = 80;
            D3D12_RESOURCE_DESC desc = tracker_texture_desc();
            ff_dx12_resource resources[count]{};

            ff_dx12_resource_tracker tracker{};
            ff_dx12_resource_tracker_init(&tracker);

            for (size_t i = 0; i < count; i++)
            {
                Assert::IsTrue(ff_dx12_resource_init_committed(&resources[i], FF_SVL("many texture"), &desc, nullptr));
                ff_dx12_resource_tracker_state(&tracker, &resources[i], D3D12_RESOURCE_STATE_COPY_DEST, 0, 0, 0, 0);
            }

            Assert::AreEqual(count, ff_array_count(tracker.entries_a));

            // Every resource must still be found after the map rehashed several times.
            for (size_t i = 0; i < count; i++)
            {
                ff_dx12_resource_tracker_state(&tracker, &resources[i], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, 0, 0, 0);
            }

            Assert::AreEqual(count, ff_array_count(tracker.entries_a));
            Assert::AreEqual(count, ff_array_count(tracker.barriers_pending_a));

            ff_dx12_resource_tracker_reset(&tracker);
            ff_dx12_resource_tracker_destroy(&tracker);

            for (size_t i = 0; i < count; i++)
            {
                ff_dx12_resource_destroy(&resources[i]);
            }
        }

        TEST_METHOD(many_first_barriers_are_not_dropped)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            // A fixed 4-entry array used to silently drop barriers past the fourth subresource.
            D3D12_RESOURCE_DESC desc = tracker_texture_desc(1, 7);
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("many mip texture"), &desc, nullptr));

            ff_dx12_resource_tracker tracker{};
            ff_dx12_resource_tracker_init(&tracker);

            ff_dx12_resource_tracker_state(&tracker, &resource, D3D12_RESOURCE_STATE_COPY_DEST, 0, 1, 0, 5);

            ff_dx12_resource_tracker_entry& entry = tracker.entries_a[0];
            Assert::AreEqual((size_t)5, ff_array_count(entry.first_barriers_a));

            for (UINT i = 0; i < 5; i++)
            {
                Assert::AreEqual(i, entry.first_barriers_a[i].Transition.Subresource);
            }

            ff_dx12_resource_tracker_reset(&tracker);
            ff_dx12_resource_tracker_destroy(&tracker);
            ff_dx12_resource_destroy(&resource);
        }

        TEST_METHOD(subresource_transitions_track_independently)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = tracker_texture_desc(1, 4);
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("mip texture"), &desc, nullptr));

            ff_dx12_resource_tracker tracker{};
            ff_dx12_resource_tracker_init(&tracker);

            ff_dx12_resource_tracker_state(&tracker, &resource, D3D12_RESOURCE_STATE_COPY_DEST, 0, 1, 0, 1);

            ff_dx12_resource_tracker_entry& entry = tracker.entries_a[0];
            Assert::AreEqual((size_t)1, ff_array_count(entry.first_barriers_a));
            Assert::AreEqual((UINT)0, entry.first_barriers_a[0].Transition.Subresource);

            Assert::AreEqual((int)D3D12_RESOURCE_STATE_COPY_DEST,
                (int)ff_dx12_resource_state_get(&entry.state, 0, nullptr).state);
            Assert::AreEqual((int)ff_dx12_resource_state_type_none,
                (int)ff_dx12_resource_state_get(&entry.state, 1, nullptr).type);

            ff_dx12_resource_tracker_reset(&tracker);
            ff_dx12_resource_tracker_destroy(&tracker);
            ff_dx12_resource_destroy(&resource);
        }

        TEST_METHOD(uav_and_alias_barriers_are_pending)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = tracker_texture_desc();
            desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("uav texture"), &desc, nullptr));

            ff_dx12_resource_tracker tracker{};
            ff_dx12_resource_tracker_init(&tracker);

            ff_dx12_resource_tracker_uav(&tracker, &resource);
            ff_dx12_resource_tracker_alias(&tracker, nullptr, &resource);

            Assert::AreEqual((size_t)2, ff_array_count(tracker.barriers_pending_a));
            Assert::AreEqual((int)D3D12_RESOURCE_BARRIER_TYPE_UAV, (int)tracker.barriers_pending_a[0].Type);
            Assert::AreEqual((int)D3D12_RESOURCE_BARRIER_TYPE_ALIASING, (int)tracker.barriers_pending_a[1].Type);

            ff_dx12_resource_tracker_reset(&tracker);
            ff_dx12_resource_tracker_destroy(&tracker);
            ff_dx12_resource_destroy(&resource);
        }

        TEST_METHOD(reset_lets_the_tracker_be_reused)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = tracker_texture_desc();
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("reused texture"), &desc, nullptr));

            ff_dx12_resource_tracker tracker{};
            ff_dx12_resource_tracker_init(&tracker);

            for (size_t i = 0; i < 4; i++)
            {
                ff_dx12_resource_tracker_state(&tracker, &resource, D3D12_RESOURCE_STATE_COPY_DEST, 0, 0, 0, 0);
                Assert::AreEqual((size_t)1, ff_array_count(tracker.entries_a));

                ff_dx12_resource_tracker_reset(&tracker);
                Assert::AreEqual((size_t)0, ff_array_count(tracker.entries_a));
                Assert::IsNull((void*)resource.tracker_references);
            }

            ff_dx12_resource_tracker_destroy(&tracker);
            ff_dx12_resource_destroy(&resource);
        }

        // The close() path below resolves first-transition barriers against the previous
        // command list. It is only reachable through a real execute, so these drive it end
        // to end rather than calling the tracker directly.

        TEST_METHOD(many_resolved_barriers_in_one_close)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            const size_t count = 200;
            static ff_dx12_resource resources[count];
            memset(resources, 0, sizeof(resources));

            D3D12_RESOURCE_DESC desc = tracker_texture_desc();

            ff_dx12_commands first{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &first));

            for (size_t i = 0; i < count; i++)
            {
                Assert::IsTrue(ff_dx12_resource_init_committed(&resources[i], FF_SVL("barrier tex"), &desc, nullptr));
                ff_dx12_commands_resource_state(&first, &resources[i], D3D12_RESOURCE_STATE_COPY_DEST, 0, 0, 0, 0);
            }

            ff_dx12_fence_value v1 = ff_dx12_queue_execute(ff_dx12_direct_queue(), &first);
            ff_dx12_fence_value_wait(v1, nullptr);

            // Textures don't decay from COPY_DEST, so every one of these resolves to a real
            // barrier emitted from a single close().
            ff_dx12_commands second{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &second));

            for (size_t i = 0; i < count; i++)
            {
                ff_dx12_commands_resource_state(&second, &resources[i], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, 0, 0, 0);
            }

            ff_dx12_fence_value v2 = ff_dx12_queue_execute(ff_dx12_direct_queue(), &second);
            ff_dx12_fence_value_wait(v2, nullptr);

            for (size_t i = 0; i < count; i++)
            {
                ff_dx12_resource_state_entry state =
                    ff_dx12_resource_state_get(ff_dx12_resource_global_state(&resources[i]), 0, nullptr);
                Assert::AreEqual((int)D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, (int)state.state);

                ff_dx12_resource_destroy(&resources[i]);
            }

            ff_dx12_wait_for_idle();
            ff_dx12_flush_keep_alive();
        }

        // 4 array slices x 4 mips is past the 8 entry inline state storage, so the state
        // has to spill into the arena while staying divergent across a close().
        TEST_METHOD(divergent_subresources_resolve_across_two_command_lists)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = tracker_texture_desc(4, 4);
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("diverge"), &desc, nullptr));

            const size_t subs = ff_dx12_resource_sub_resource_size(&resource);
            Assert::AreEqual((size_t)16, subs);

            ff_dx12_commands a{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &a));

            for (size_t i = 0; i < subs; i++)
            {
                ff_dx12_commands_resource_state_sub_index(&a, &resource,
                    (i & 1) ? D3D12_RESOURCE_STATE_COPY_DEST : D3D12_RESOURCE_STATE_COPY_SOURCE, i);
            }

            ff_dx12_commands b{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &b));

            for (size_t i = 0; i < subs; i++)
            {
                ff_dx12_commands_resource_state_sub_index(&b, &resource,
                    (i & 1) ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COPY_DEST, i);
            }

            ff_dx12_commands* both[] = { &a, &b };
            ff_dx12_queue_execute_many(ff_dx12_direct_queue(), both, 2);
            ff_dx12_wait_for_idle();

            ff_dx12_resource_state* global = ff_dx12_resource_global_state(&resource);
            for (size_t i = 0; i < subs; i++)
            {
                ff_dx12_resource_state_entry state = ff_dx12_resource_state_get(global, i, nullptr);
                Assert::AreEqual(
                    (int)((i & 1) ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COPY_DEST),
                    (int)state.state);
            }

            ff_dx12_resource_destroy(&resource);
            ff_dx12_wait_for_idle();
            ff_dx12_flush_keep_alive();
        }

        // A whole-resource transition resolved against a divergent previous state has to be
        // split back into per-subresource barriers instead of staying ALL_SUBRESOURCES.
        TEST_METHOD(all_subresources_barrier_splits_against_divergent_previous_state)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = tracker_texture_desc(2, 4);
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("split"), &desc, nullptr));

            const size_t subs = ff_dx12_resource_sub_resource_size(&resource);

            ff_dx12_commands a{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &a));

            for (size_t i = 0; i < subs; i++)
            {
                ff_dx12_commands_resource_state_sub_index(&a, &resource,
                    (i < subs / 2) ? D3D12_RESOURCE_STATE_COPY_DEST : D3D12_RESOURCE_STATE_COPY_SOURCE, i);
            }

            ff_dx12_commands b{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &b));
            ff_dx12_commands_resource_state(&b, &resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, 0, 0, 0);

            ff_dx12_commands* both[] = { &a, &b };
            ff_dx12_queue_execute_many(ff_dx12_direct_queue(), both, 2);
            ff_dx12_wait_for_idle();

            ff_dx12_resource_state* global = ff_dx12_resource_global_state(&resource);
            for (size_t i = 0; i < subs; i++)
            {
                ff_dx12_resource_state_entry state = ff_dx12_resource_state_get(global, i, nullptr);
                Assert::AreEqual((int)D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, (int)state.state);
            }

            ff_dx12_resource_destroy(&resource);
            ff_dx12_wait_for_idle();
            ff_dx12_flush_keep_alive();
        }

        TEST_METHOD(forget_keeps_the_remaining_resources_findable)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            // Resources in a plain array have a fixed stride, which the multiplicative pointer hash
            // turns into a collision-free permutation of slots. That never exercises a probe chain,
            // so the addresses are jittered by odd-sized padding allocations to force real
            // collisions and multi-entry runs.
            const size_t count = 128;
            ff_dx12_resource* resources[count]{};
            void* padding[count]{};
            bool forgotten[count]{};
            uint32_t rand_state = 12345;

            D3D12_RESOURCE_DESC desc = tracker_texture_desc();

            ff_dx12_resource_tracker tracker{};
            ff_dx12_resource_tracker_init(&tracker);

            for (size_t i = 0; i < count; i++)
            {
                rand_state = rand_state * 1664525u + 1013904223u;
                padding[i] = ::malloc(8 + ((rand_state >> 16) % 500) * 8);
                Assert::IsNotNull(padding[i]);

                resources[i] = (ff_dx12_resource*)::calloc(1, sizeof(ff_dx12_resource));
                Assert::IsNotNull(resources[i]);
                Assert::IsTrue(ff_dx12_resource_init_committed(resources[i], FF_SVL("forget texture"), &desc, nullptr));
                ff_dx12_resource_tracker_state(&tracker, resources[i], D3D12_RESOURCE_STATE_COPY_DEST, 0, 0, 0, 0);
            }

            Assert::AreEqual(count, ff_array_count(tracker.entries_a));

            // Forget one at a time, and after each removal re-look-up every survivor. A survivor
            // stranded behind a cleared slot is not found and silently gets a second entry, so the
            // entry count is the detector.
            size_t live = count;
            for (size_t i = 0; i < count; i++)
            {
                ff_dx12_resource_tracker_forget(&tracker, resources[i]);
                forgotten[i] = true;
                live--;

                Assert::IsNull((void*)resources[i]->tracker_references, L"a forgotten resource must drop its tracker reference");
                Assert::AreEqual(live, ff_array_count(tracker.entries_a));

                for (size_t j = 0; j < count; j++)
                {
                    if (!forgotten[j])
                    {
                        // Same state as before, so this is a pure lookup with no barrier side effect.
                        ff_dx12_resource_tracker_state(&tracker, resources[j], D3D12_RESOURCE_STATE_COPY_DEST, 0, 0, 0, 0);
                    }
                }

                Assert::AreEqual(live, ff_array_count(tracker.entries_a),
                    L"a survivor was stranded by a broken probe chain and got a duplicate entry");
            }

            Assert::AreEqual((size_t)0, ff_array_count(tracker.entries_a));
            Assert::AreEqual((size_t)0, ff_array_count(tracker.barriers_pending_a));

            ff_dx12_resource_tracker_reset(&tracker);
            ff_dx12_resource_tracker_destroy(&tracker);

            for (size_t i = 0; i < count; i++)
            {
                ff_dx12_resource_destroy(resources[i]);
                ::free(resources[i]);
                ::free(padding[i]);
            }
        }

        TEST_METHOD(forget_then_retrack_makes_a_fresh_entry)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = tracker_texture_desc();
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("retrack"), &desc, nullptr));

            ff_dx12_resource_tracker tracker{};
            ff_dx12_resource_tracker_init(&tracker);

            ff_dx12_resource_tracker_state(&tracker, &resource, D3D12_RESOURCE_STATE_COPY_DEST, 0, 0, 0, 0);
            ff_dx12_resource_tracker_forget(&tracker, &resource);
            Assert::AreEqual((size_t)0, ff_array_count(tracker.entries_a));

            // The slot the forgotten entry used must be reusable, not poisoned by a stale index.
            ff_dx12_resource_tracker_state(&tracker, &resource, D3D12_RESOURCE_STATE_COPY_DEST, 0, 0, 0, 0);

            Assert::AreEqual((size_t)1, ff_array_count(tracker.entries_a));
            Assert::IsTrue(tracker.entries_a[0].resource == &resource);

            // A fresh entry means this is a first transition again, so it is held back rather than
            // becoming a pending barrier.
            Assert::AreEqual((size_t)0, ff_array_count(tracker.barriers_pending_a));
            Assert::AreEqual((size_t)1, ff_array_count(tracker.entries_a[0].first_barriers_a));

            ff_dx12_resource_tracker_reset(&tracker);
            ff_dx12_resource_tracker_destroy(&tracker);
            ff_dx12_resource_destroy(&resource);
        }

        TEST_METHOD(destroyed_resource_state_flows_through_unreferencing_trackers)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = tracker_texture_desc();
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("destroyed tracker chain"),
                &desc, nullptr));
            ID3D12Resource* resource_identity = resource.resource;

            ff_dx12_resource_tracker first{};
            ff_dx12_resource_tracker middle{};
            ff_dx12_resource_tracker last{};
            ff_dx12_resource_tracker_init(&first);
            ff_dx12_resource_tracker_init(&middle);
            ff_dx12_resource_tracker_init(&last);

            ff_dx12_resource_tracker_state(&first, &resource, D3D12_RESOURCE_STATE_RENDER_TARGET, 0, 0, 0, 0);
            ff_dx12_resource_tracker_state(&last, &resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, 0, 0, 0);
            ff_dx12_resource_destroy(&resource);

            tracker_barrier_recorder first_list;
            tracker_barrier_recorder middle_list;
            tracker_barrier_recorder last_list;
            ff_dx12_resource_tracker_close(&first, &first_list, nullptr, &middle);
            ff_dx12_resource_tracker_close(&middle, &middle_list, &first, &last);
            ff_dx12_resource_tracker_close(&last, &last_list, &middle, nullptr);

            Assert::AreEqual((size_t)1, first_list.count);
            assert_tracker_barrier(first_list.barriers[0], resource_identity,
                D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
            Assert::AreEqual((size_t)1, last_list.count);
            assert_tracker_barrier(last_list.barriers[0], resource_identity,
                D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

            ff_dx12_resource_tracker_destroy(&first);
            ff_dx12_resource_tracker_destroy(&middle);
            ff_dx12_resource_tracker_destroy(&last);
        }

        TEST_METHOD(forgotten_common_buffer_does_not_use_replacement_state)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc{};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            desc.Width = 1024;
            desc.Height = 1;
            desc.DepthOrArraySize = 1;
            desc.MipLevels = 1;
            desc.SampleDesc.Count = 1;
            desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("old buffer"), &desc, nullptr));
            ID3D12Resource* old_resource = resource.resource;

            ff_dx12_resource_tracker a{};
            ff_dx12_resource_tracker b{};
            ff_dx12_resource_tracker_init(&a);
            ff_dx12_resource_tracker_init(&b);
            tracker_barrier_recorder recorder;

            ff_dx12_resource_tracker_state(&b, &resource, D3D12_RESOURCE_STATE_COPY_DEST, 0, 0, 0, 0);
            ff_dx12_resource_destroy(&resource);
            desc.Width *= 2;
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("replacement buffer"), &desc, nullptr));
            Assert::IsTrue(resource.resource != old_resource);

            ff_dx12_resource_tracker_state(&b, &resource, D3D12_RESOURCE_STATE_COPY_DEST, 0, 0, 0, 0);
            ff_dx12_resource_tracker_state(&b, &resource, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, 0, 0, 0, 0);
            ff_dx12_resource_tracker_flush(&b, &recorder);
            Assert::AreEqual((size_t)1, recorder.count);
            assert_tracker_barrier(recorder.barriers[0], resource.resource,
                D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

            recorder.count = 0;
            ff_dx12_resource_tracker_close(&a, &recorder, nullptr, &b);
            ff_dx12_resource_tracker_close(&b, &recorder, &a, nullptr);
            Assert::AreEqual((size_t)0, recorder.count,
                L"Both allocations promote from COMMON; neither needs a first-use barrier");

            ff_dx12_resource_tracker_destroy(&b);
            ff_dx12_resource_tracker_destroy(&a);
            ff_dx12_resource_destroy(&resource);
        }

        TEST_METHOD(forgotten_required_barrier_survives_wrapper_reuse)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = tracker_texture_desc();
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("old texture"), &desc, nullptr));
            ID3D12Resource* old_resource = resource.resource;
            ff_dx12_resource_state_set(ff_dx12_resource_global_state(&resource), D3D12_RESOURCE_STATE_COPY_DEST,
                ff_dx12_resource_state_type_global, 0, 1);

            ff_dx12_resource_tracker a{};
            ff_dx12_resource_tracker b{};
            ff_dx12_resource_tracker_init(&a);
            ff_dx12_resource_tracker_init(&b);
            tracker_barrier_recorder recorder;

            ff_dx12_resource_tracker_state(&b, &resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, 0, 0, 0);
            ff_dx12_resource_destroy(&resource);
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("replacement texture"), &desc, nullptr));
            Assert::IsTrue(resource.resource != old_resource);
            ff_dx12_resource_tracker_state(&b, &resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, 0, 0, 0);

            ff_dx12_resource_tracker_close(&a, &recorder, nullptr, &b);
            ff_dx12_resource_tracker_close(&b, &recorder, &a, nullptr);
            Assert::AreEqual((size_t)1, recorder.count);
            assert_tracker_barrier(recorder.barriers[0], old_resource,
                D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

            ff_dx12_resource_tracker_destroy(&b);
            ff_dx12_resource_tracker_destroy(&a);
            ff_dx12_resource_destroy(&resource);
        }

        TEST_METHOD(forgotten_barrier_uses_previous_state_before_live_merge)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = tracker_texture_desc();
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("previous texture"), &desc, nullptr));
            ID3D12Resource* old_resource = resource.resource;

            ff_dx12_resource_tracker a{};
            ff_dx12_resource_tracker b{};
            ff_dx12_resource_tracker_init(&a);
            ff_dx12_resource_tracker_init(&b);
            tracker_barrier_recorder recorder;

            ff_dx12_resource_tracker_state(&a, &resource, D3D12_RESOURCE_STATE_COPY_DEST, 0, 0, 0, 0);
            ff_dx12_resource_tracker_close(&a, &recorder, nullptr, &b);
            Assert::AreEqual((size_t)0, recorder.count);

            ff_dx12_resource_tracker_state(&b, &resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, 0, 0, 0);
            ff_dx12_resource_destroy(&resource);
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("replacement texture"), &desc, nullptr));
            Assert::IsTrue(resource.resource != old_resource);
            ff_dx12_resource_tracker_state(&b, &resource, D3D12_RESOURCE_STATE_COPY_SOURCE, 0, 0, 0, 0);
            ff_dx12_resource_tracker_close(&b, &recorder, &a, nullptr);

            Assert::AreEqual((size_t)1, recorder.count);
            assert_tracker_barrier(recorder.barriers[0], old_resource,
                D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

            ff_dx12_resource_tracker_destroy(&b);
            ff_dx12_resource_tracker_destroy(&a);
            ff_dx12_resource_destroy(&resource);
        }

        TEST_METHOD(forgotten_barrier_ignores_previous_entry_for_another_allocation)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = tracker_texture_desc();
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("forgotten texture"), &desc, nullptr));
            ID3D12Resource* old_resource = resource.resource;
            ff_dx12_resource_state_set(ff_dx12_resource_global_state(&resource), D3D12_RESOURCE_STATE_COPY_DEST,
                ff_dx12_resource_state_type_global, 0, 1);

            ff_dx12_resource_tracker a{};
            ff_dx12_resource_tracker b{};
            ff_dx12_resource_tracker_init(&a);
            ff_dx12_resource_tracker_init(&b);
            tracker_barrier_recorder recorder;

            ff_dx12_resource_tracker_state(&b, &resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, 0, 0, 0);
            ff_dx12_resource_destroy(&resource);
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("earlier replacement"), &desc, nullptr));
            Assert::IsTrue(resource.resource != old_resource);
            ff_dx12_resource_tracker_state(&a, &resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, 0, 0, 0);
            ff_dx12_resource_tracker_close(&a, &recorder, nullptr, &b);
            Assert::AreEqual((size_t)0, recorder.count);

            ff_dx12_resource_tracker_close(&b, &recorder, &a, nullptr);
            Assert::AreEqual((size_t)1, recorder.count);
            assert_tracker_barrier(recorder.barriers[0], old_resource,
                D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

            ff_dx12_resource_tracker_destroy(&b);
            ff_dx12_resource_tracker_destroy(&a);
            ff_dx12_resource_destroy(&resource);
        }

        // A texture starting in COMMON promotes straight to a read state, so the first use in a
        // command list must not emit a barrier at all.
        TEST_METHOD(texture_promotes_from_common_to_a_read_state)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = tracker_texture_desc();
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("promote read"), &desc, nullptr));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));
            ff_dx12_commands_resource_state(&commands, &resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, 0, 0, 0);

            ff_dx12_fence_value v = ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_fence_value_wait(v, nullptr);

            // Promoted to a read state, so it decays back to COMMON at ExecuteCommandLists.
            ff_dx12_resource_state_entry state =
                ff_dx12_resource_state_get(ff_dx12_resource_global_state(&resource), 0, nullptr);
            Assert::AreEqual((int)D3D12_RESOURCE_STATE_COMMON, (int)state.state);

            ff_dx12_resource_destroy(&resource);
            ff_dx12_wait_for_idle();
            ff_dx12_flush_keep_alive();
        }

        // COPY_DEST is a write state, so a texture promoted into it does NOT decay. This is the
        // asymmetry that makes the second command list need a real barrier.
        TEST_METHOD(texture_does_not_decay_from_a_write_state)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = tracker_texture_desc();
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("no decay"), &desc, nullptr));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));
            ff_dx12_commands_resource_state(&commands, &resource, D3D12_RESOURCE_STATE_COPY_DEST, 0, 0, 0, 0);

            ff_dx12_fence_value v = ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_fence_value_wait(v, nullptr);

            ff_dx12_resource_state_entry state =
                ff_dx12_resource_state_get(ff_dx12_resource_global_state(&resource), 0, nullptr);
            Assert::AreEqual((int)D3D12_RESOURCE_STATE_COPY_DEST, (int)state.state,
                L"a texture promoted to a write state must stay in it across ExecuteCommandLists");

            ff_dx12_resource_destroy(&resource);
            ff_dx12_wait_for_idle();
            ff_dx12_flush_keep_alive();
        }

        // Buffers decay from any state, including write states, unlike non-simultaneous-access
        // textures. Getting this wrong would leave a stale non-COMMON global state on every
        // upload buffer.
        TEST_METHOD(buffer_decays_from_a_write_state)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc{};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            desc.Width = 1024;
            desc.Height = 1;
            desc.DepthOrArraySize = 1;
            desc.MipLevels = 1;
            desc.Format = DXGI_FORMAT_UNKNOWN;
            desc.SampleDesc.Count = 1;
            desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("decay buffer"), &desc, nullptr));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));
            ff_dx12_commands_resource_state(&commands, &resource, D3D12_RESOURCE_STATE_COPY_DEST, 0, 0, 0, 0);

            ff_dx12_fence_value v = ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_fence_value_wait(v, nullptr);

            ff_dx12_resource_state_entry state =
                ff_dx12_resource_state_get(ff_dx12_resource_global_state(&resource), 0, nullptr);
            Assert::AreEqual((int)D3D12_RESOURCE_STATE_COMMON, (int)state.state,
                L"buffers decay to COMMON from any state");

            ff_dx12_resource_destroy(&resource);
            ff_dx12_wait_for_idle();
            ff_dx12_flush_keep_alive();
        }

        // A texture can be promoted from COMMON to several read states at once, but only to a
        // single write state. A combined read mask must promote rather than emit a barrier.
        TEST_METHOD(texture_promotes_to_combined_read_states)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = tracker_texture_desc();
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("combined read"), &desc, nullptr));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));
            ff_dx12_commands_resource_state(&commands, &resource,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, 0, 0, 0, 0);

            ff_dx12_fence_value v = ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_fence_value_wait(v, nullptr);

            ff_dx12_resource_state_entry state =
                ff_dx12_resource_state_get(ff_dx12_resource_global_state(&resource), 0, nullptr);
            Assert::AreEqual((int)D3D12_RESOURCE_STATE_COMMON, (int)state.state,
                L"a combined read promotion still decays back to COMMON");

            ff_dx12_resource_destroy(&resource);
            ff_dx12_wait_for_idle();
            ff_dx12_flush_keep_alive();
        }

        // A resource left in a write state by one command list, then read by the next, is the
        // case where promotion must NOT happen and a real barrier is required.
        TEST_METHOD(write_then_read_across_lists_emits_a_barrier)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = tracker_texture_desc();
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("write then read"), &desc, nullptr));

            ff_dx12_commands a{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &a));
            ff_dx12_commands_resource_state(&a, &resource, D3D12_RESOURCE_STATE_COPY_DEST, 0, 0, 0, 0);

            ff_dx12_commands b{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &b));
            ff_dx12_commands_resource_state(&b, &resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, 0, 0, 0);

            ff_dx12_commands* both[] = { &a, &b };
            ff_dx12_queue_execute_many(ff_dx12_direct_queue(), both, 2);
            ff_dx12_wait_for_idle();

            // Reached PIXEL_SHADER_RESOURCE via a real barrier, not promotion, so it is type
            // 'barrier' and does not decay.
            ff_dx12_resource_state_entry state =
                ff_dx12_resource_state_get(ff_dx12_resource_global_state(&resource), 0, nullptr);
            Assert::AreEqual((int)D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, (int)state.state,
                L"a barrier-reached read state must not decay to COMMON");

            ff_dx12_resource_destroy(&resource);
            ff_dx12_wait_for_idle();
            ff_dx12_flush_keep_alive();
        }

        // After decay the state is COMMON again, so the next command list promotes into it for
        // free rather than paying a redundant COMMON -> read barrier.
        //
        // The global state's *type* deliberately stays 'global' forever: merge_entry only takes
        // the state value when the destination is global. That is load-bearing, because
        // allow_promotion only promotes when the previous type is global and the state is COMMON.
        // A side effect is that 'decayed' is written into the tracker entry but never survives
        // into the global state.
        TEST_METHOD(decayed_resource_promotes_again_on_next_use)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = tracker_texture_desc();
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("redecay"), &desc, nullptr));

            ff_dx12_commands a{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &a));
            ff_dx12_commands_resource_state(&a, &resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, 0, 0, 0);
            ff_dx12_fence_value v1 = ff_dx12_queue_execute(ff_dx12_direct_queue(), &a);
            ff_dx12_fence_value_wait(v1, nullptr);

            ff_dx12_resource_state_entry after_decay =
                ff_dx12_resource_state_get(ff_dx12_resource_global_state(&resource), 0, nullptr);

            // Second use of the same read state. Since the resource is back in COMMON this must
            // promote again, which means it decays again and lands back in COMMON.
            ff_dx12_commands b{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &b));
            ff_dx12_commands_resource_state(&b, &resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, 0, 0, 0);
            ff_dx12_fence_value v2 = ff_dx12_queue_execute(ff_dx12_direct_queue(), &b);
            ff_dx12_fence_value_wait(v2, nullptr);

            ff_dx12_resource_state_entry second =
                ff_dx12_resource_state_get(ff_dx12_resource_global_state(&resource), 0, nullptr);

            ff_dx12_resource_destroy(&resource);
            ff_dx12_wait_for_idle();
            ff_dx12_flush_keep_alive();

            Assert::AreEqual((int)D3D12_RESOURCE_STATE_COMMON, (int)after_decay.state);
            Assert::AreEqual((int)ff_dx12_resource_state_type_global, (int)after_decay.type,
                L"the global state keeps type 'global' so allow_promotion can fire again");

            Assert::AreEqual((int)D3D12_RESOURCE_STATE_COMMON, (int)second.state,
                L"a decayed resource must promote again, not take a barrier that leaves it non-COMMON");
            Assert::AreEqual((int)ff_dx12_resource_state_type_global, (int)second.type);
        }

        TEST_METHOD(repeat_transitions_stay_on_the_all_same_fast_path)        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            // The sprite renderer rebinds the same index buffer and textures every flush. That is
            // only cheap because a repeat transition collapses to the all_same early-out: one
            // lookup, no barrier, no per-subresource walk. This pins that property.
            D3D12_RESOURCE_DESC desc = tracker_texture_desc(2, 4);
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("fast path"), &desc, nullptr));

            ff_dx12_resource_tracker tracker{};
            ff_dx12_resource_tracker_init(&tracker);

            ff_dx12_resource_tracker_state(&tracker, &resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, 0, 0, 0);
            ff_dx12_resource_tracker_state(&tracker, &resource, D3D12_RESOURCE_STATE_COPY_DEST, 0, 0, 0, 0);
            Assert::AreEqual((size_t)1, ff_array_count(tracker.barriers_pending_a));

            // The state is uniform across subresources, so it must stay collapsed to one entry
            // rather than expanding to one per subresource.
            Assert::IsTrue(ff_dx12_resource_state_all_same(&tracker.entries_a[0].state));
            Assert::AreEqual((size_t)1, tracker.entries_a[0].state.count);

            for (size_t i = 0; i < 64; i++)
            {
                ff_dx12_resource_tracker_state(&tracker, &resource, D3D12_RESOURCE_STATE_COPY_DEST, 0, 0, 0, 0);
            }

            Assert::AreEqual((size_t)1, ff_array_count(tracker.barriers_pending_a),
                L"repeat transitions to the current state must not emit barriers");
            Assert::AreEqual((size_t)1, ff_array_count(tracker.entries_a));
            Assert::AreEqual((size_t)1, tracker.entries_a[0].state.count,
                L"the state must stay collapsed instead of expanding per subresource");
            Assert::IsNull((void*)tracker.entries_a[0].state.overflow,
                L"the fast path must not spill subresource storage into the arena");

            ff_dx12_resource_tracker_reset(&tracker);
            ff_dx12_resource_tracker_destroy(&tracker);
            ff_dx12_resource_destroy(&resource);
        }
    };
}
