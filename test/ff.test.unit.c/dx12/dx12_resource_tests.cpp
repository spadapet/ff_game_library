#include "pch.h"

namespace ff::test::dx12
{
    static D3D12_RESOURCE_DESC texture_desc(UINT16 array_size = 1, UINT16 mip_levels = 1)
    {
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = 64;
        desc.Height = 64;
        desc.DepthOrArraySize = array_size;
        desc.MipLevels = mip_levels;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        return desc;
    }

    static D3D12_RESOURCE_DESC buffer_desc(UINT64 size = 1024)
    {
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = size;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        return desc;
    }

    TEST_CLASS(dx12_resource_tests)
    {
    public:
        TEST_METHOD_CLEANUP(cleanup)
        {
            ff_dx12_destroy();
        }

        TEST_METHOD(committed_resource_init_and_destroy)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = texture_desc();
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("committed texture"), &desc, nullptr));

            Assert::IsTrue(ff_dx12_resource_valid(&resource));
            Assert::AreEqual((size_t)1, ff_dx12_resource_array_size(&resource));
            Assert::AreEqual((size_t)1, ff_dx12_resource_mip_size(&resource));
            Assert::AreEqual((size_t)1, ff_dx12_resource_sub_resource_size(&resource));
            Assert::IsNotNull((void*)ff_dx12_resource_residency_data(&resource));

            ff_dx12_resource_destroy(&resource);
            Assert::IsFalse(ff_dx12_resource_valid(&resource));
        }

        TEST_METHOD(committed_buffer_has_a_gpu_address)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = buffer_desc();
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("committed buffer"), &desc, nullptr));
            Assert::IsTrue(ff_dx12_resource_gpu_address(&resource) != 0);

            ff_dx12_resource_destroy(&resource);
        }

        TEST_METHOD(array_and_mip_sizes_come_from_the_desc)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = texture_desc(4, 3);
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("array texture"), &desc, nullptr));

            Assert::AreEqual((size_t)4, ff_dx12_resource_array_size(&resource));
            Assert::AreEqual((size_t)3, ff_dx12_resource_mip_size(&resource));
            Assert::AreEqual((size_t)12, ff_dx12_resource_sub_resource_size(&resource));

            ff_dx12_resource_destroy(&resource);
        }

        TEST_METHOD(placed_resource_uses_the_supplied_mem_range)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_mem_allocator allocator{};
            ff_dx12_mem_allocator_init(&allocator, 1024 * 1024, 0, ff_dx12_heap_usage_gpu_textures, false);

            D3D12_RESOURCE_DESC desc = texture_desc();
            D3D12_RESOURCE_ALLOCATION_INFO info = ff_dx12_device()->GetResourceAllocationInfo(0, 1, &desc);

            ff_dx12_mem_range range = ff_dx12_mem_allocator_alloc_bytes(&allocator, info.SizeInBytes, info.Alignment);
            Assert::IsTrue(ff_dx12_mem_range_valid(&range));

            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_placed(&resource, FF_SVL("placed texture"), &range, &desc, nullptr));
            Assert::IsTrue(ff_dx12_resource_valid(&resource));
            Assert::IsNotNull((void*)ff_dx12_resource_residency_data(&resource));

            // The resource took ownership of the range and frees it on destroy.
            ff_dx12_resource_destroy(&resource);
            ff_dx12_mem_allocator_destroy(&allocator);
        }

        TEST_METHOD(global_state_starts_common_and_global)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = texture_desc();
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("state texture"), &desc, nullptr));

            ff_dx12_resource_state* state = ff_dx12_resource_global_state(&resource);
            ff_dx12_resource_state_entry entry = ff_dx12_resource_state_get(state, 0, nullptr);
            Assert::AreEqual((int)D3D12_RESOURCE_STATE_COMMON, (int)entry.state);
            Assert::AreEqual((int)ff_dx12_resource_state_type_global, (int)entry.type);

            ff_dx12_resource_destroy(&resource);
        }

        TEST_METHOD(create_shader_view_writes_a_descriptor)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_cpu_descriptor_allocator descriptors{};
            ff_dx12_cpu_descriptor_allocator_init(&descriptors, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 16);
            ff_dx12_descriptor_range view_range = ff_dx12_cpu_descriptor_allocator_alloc(&descriptors, 1);
            Assert::IsTrue(ff_dx12_descriptor_range_valid(&view_range));

            D3D12_RESOURCE_DESC desc = texture_desc();
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("srv texture"), &desc, nullptr));

            ff_dx12_resource_create_shader_view(&resource, ff_dx12_descriptor_range_cpu_handle(&view_range, 0), 0, 0, 0, 0);

            ff_dx12_resource_destroy(&resource);
            ff_dx12_descriptor_range_free(&view_range);
            ff_dx12_cpu_descriptor_allocator_destroy(&descriptors);
        }

        TEST_METHOD(create_target_view_writes_a_descriptor)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_cpu_descriptor_allocator descriptors{};
            ff_dx12_cpu_descriptor_allocator_init(&descriptors, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 16);
            ff_dx12_descriptor_range view_range = ff_dx12_cpu_descriptor_allocator_alloc(&descriptors, 1);
            Assert::IsTrue(ff_dx12_descriptor_range_valid(&view_range));

            D3D12_RESOURCE_DESC desc = texture_desc();
            desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("rtv texture"), &desc, nullptr));

            ff_dx12_resource_create_target_view(&resource, ff_dx12_descriptor_range_cpu_handle(&view_range, 0), 0, 1, 0);

            ff_dx12_resource_destroy(&resource);
            ff_dx12_descriptor_range_free(&view_range);
            ff_dx12_cpu_descriptor_allocator_destroy(&descriptors);
        }
        TEST_METHOD(destroy_waits_for_pending_gpu_work)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = texture_desc();

            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("pending texture"), &desc, nullptr));

            ff_dx12_fence fence{};
            Assert::IsTrue(ff_dx12_fence_init(&fence, FF_SVL("pending fence"), 1));

            ff_dx12_resource_tracker tracker{};
            ff_dx12_resource_tracker_init(&tracker);

            ff_dx12_fence_values wait_before_execute{};
            ff_dx12_fence_values_init(&wait_before_execute);

            // signal_later leaves the value pending, so the resource records GPU work that has
            // not completed. Destroy must block on it rather than releasing underneath the GPU.
            ff_dx12_fence_value pending = ff_dx12_fence_signal_later(&fence);
            ff_dx12_resource_prepare_state(&resource, &wait_before_execute, pending, &tracker,
                D3D12_RESOURCE_STATE_COPY_DEST, 0, 0, 0, 0);

            Assert::IsFalse(ff_dx12_fence_value_complete(resource.global_write));

            ff_dx12_fence_signal_value(&fence, pending.value, nullptr);

            ff_dx12_resource_set_tracker(&resource, nullptr);
            ff_dx12_resource_destroy(&resource);

            ff_dx12_resource_tracker_destroy(&tracker);
            ff_dx12_fence_destroy(&fence);
        }
    };
}
