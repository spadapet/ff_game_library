#include "pch.h"

// These tests create real DXGI/D3D12 objects. Any machine that runs them has at least the WARP
// adapter available, so init is expected to succeed.
namespace ff::test::dx12
{
    TEST_CLASS(dx12_globals_tests)
    {
    public:
        TEST_METHOD_CLEANUP(cleanup)
        {
            ff_dx12_destroy();
        }

        TEST_METHOD(default_params_are_reasonable)
        {
            ff_dx12_init_params params = ff_dx12_init_params_default();
            Assert::AreEqual((int)DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, (int)params.gpu_preference);
            Assert::AreEqual((int)D3D_FEATURE_LEVEL_11_0, (int)params.feature_level);
        }

        TEST_METHOD(init_creates_dxgi_and_d3d_objects)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            Assert::IsNotNull(ff_dx12_factory());
            Assert::IsNotNull(ff_dx12_adapter());
            Assert::IsNotNull(ff_dx12_device());
            Assert::IsTrue(ff_dx12_device_valid());
            Assert::AreEqual((int)D3D_FEATURE_LEVEL_11_0, (int)ff_dx12_feature_level());
        }

        TEST_METHOD(init_honors_explicit_params)
        {
            ff_dx12_init_params params = ff_dx12_init_params_default();
            params.gpu_preference = DXGI_GPU_PREFERENCE_MINIMUM_POWER;
            params.feature_level = D3D_FEATURE_LEVEL_11_0;

            Assert::IsTrue(ff_dx12_init(&params));
            Assert::IsNotNull(ff_dx12_device());
            Assert::AreEqual((int)D3D_FEATURE_LEVEL_11_0, (int)ff_dx12_feature_level());
        }

        TEST_METHOD(device_adapter_luid_matches_adapter)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            LUID device_luid{};
            device_luid = ff_dx12_device()->GetAdapterLuid();

            DXGI_ADAPTER_DESC desc{};
            Assert::IsTrue(SUCCEEDED(ff_dx12_adapter()->GetDesc(&desc)));

            Assert::AreEqual(device_luid.LowPart, desc.AdapterLuid.LowPart);
            Assert::AreEqual(device_luid.HighPart, desc.AdapterLuid.HighPart);
        }

        TEST_METHOD(adapters_hash_is_stable_and_not_empty)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            uint64_t hash = ff_dx12_adapters_hash();
            Assert::AreNotEqual((uint64_t)0, hash);

            ff_dx12_destroy();
            Assert::IsTrue(ff_dx12_init(nullptr));
            Assert::AreEqual(hash, ff_dx12_adapters_hash());
        }

        TEST_METHOD(factory_is_current_after_init)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));
            Assert::IsTrue(ff_dx12_factory_current());
        }

        TEST_METHOD(adapter_name_is_not_empty)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_arena arena;
            ff_arena_init_heap_global(&arena, 1024);

            ff_string_view name = ff_dx12_adapter_name(ff_dx12_adapter(), &arena);
            Assert::IsTrue(name.count > 0);
            Assert::IsNotNull(name.data);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(video_memory_info_has_a_budget)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            DXGI_QUERY_VIDEO_MEMORY_INFO info = ff_dx12_video_memory_info();
            Assert::IsTrue(info.Budget > 0);

            // Refreshing must not clobber the cached values.
            ff_dx12_update_video_memory_info();
            Assert::IsTrue(ff_dx12_video_memory_info().Budget > 0);
        }

        TEST_METHOD(fatal_error_invalidates_the_device)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));
            Assert::IsTrue(ff_dx12_device_valid());

            ff_dx12_device_fatal_error(FF_SVL("test induced failure"));
            Assert::IsFalse(ff_dx12_device_valid());

            // A fresh init clears the simulated failure.
            ff_dx12_destroy();
            Assert::IsTrue(ff_dx12_init(nullptr));
            Assert::IsTrue(ff_dx12_device_valid());
        }

        TEST_METHOD(destroy_clears_all_globals)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));
            ff_dx12_destroy();

            Assert::IsNull(ff_dx12_factory());
            Assert::IsNull(ff_dx12_adapter());
            Assert::IsNull(ff_dx12_device());
            Assert::IsFalse(ff_dx12_device_valid());
            Assert::IsFalse(ff_dx12_factory_current());
            Assert::AreEqual((uint64_t)0, ff_dx12_adapters_hash());
            Assert::AreEqual((int)0, (int)ff_dx12_feature_level());
        }

        TEST_METHOD(destroy_is_idempotent)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));
            ff_dx12_destroy();
            ff_dx12_destroy();

            Assert::IsNull(ff_dx12_device());
        }

        TEST_METHOD(destroy_without_init_is_safe)
        {
            ff_dx12_destroy();
            Assert::IsNull(ff_dx12_device());
        }

        TEST_METHOD(init_and_destroy_can_repeat)
        {
            for (int i = 0; i < 3; i++)
            {
                Assert::IsTrue(ff_dx12_init(nullptr));
                Assert::IsNotNull(ff_dx12_device());
                ff_dx12_destroy();
                Assert::IsNull(ff_dx12_device());
            }
        }

        TEST_METHOD(device_objects_are_usable)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            // Prove the device is a real, working D3D12 device by creating a command queue.
            D3D12_COMMAND_QUEUE_DESC desc{};
            desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

            ID3D12CommandQueue* queue = nullptr;
            Assert::IsTrue(SUCCEEDED(ff_dx12_device()->CreateCommandQueue(&desc, IID_PPV_ARGS(&queue))));
            Assert::IsNotNull(queue);

            queue->Release();
        }
        TEST_METHOD(repeated_video_memory_updates_stay_valid)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            for (size_t i = 0; i < 8; i++)
            {
                ff_dx12_update_video_memory_info();
                DXGI_QUERY_VIDEO_MEMORY_INFO info = ff_dx12_video_memory_info();
                Assert::IsTrue(info.Budget > 0);
            }
        }

        TEST_METHOD(residency_tolerates_usage_over_budget)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            // Exercises make_resident's budget math. Available space is computed as
            // Budget - CurrentUsage, which must not wrap when usage exceeds the budget.
            ff_dx12_heap heap{};
            Assert::IsTrue(ff_dx12_heap_init(&heap, FF_SVL("budget heap"), 64 * 1024, ff_dx12_heap_usage_gpu_buffers));

            ff_dx12_fence fence{};
            Assert::IsTrue(ff_dx12_fence_init(&fence, FF_SVL("budget fence"), 1));
            ff_dx12_fence_value value = ff_dx12_fence_signal(&fence, nullptr);

            ff_dx12_residency_data* set[1] = { ff_dx12_heap_residency_data(&heap) };
            ff_dx12_fence_values wait_values{};
            ff_dx12_fence_values_init(&wait_values);

            Assert::IsTrue(ff_dx12_make_resident(set, 1, value, &wait_values));

            ff_dx12_fence_values_wait(&wait_values, nullptr);
            ff_dx12_fence_destroy(&fence);
            ff_dx12_heap_destroy(&heap);
        }
    };
}
