#include "pch.h"

namespace ff::test::dx12
{
    TEST_CLASS(dx12_heap_tests)
    {
    public:
        TEST_METHOD_CLEANUP(cleanup)
        {
            ff_dx12_destroy();
        }

        TEST_METHOD(upload_heap_has_mapped_cpu_data)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_heap heap{};
            Assert::IsTrue(ff_dx12_heap_init(&heap, nullptr, FF_SVL("upload heap"), 64 * 1024, ff_dx12_heap_usage_upload));
            Assert::IsTrue(ff_dx12_heap_valid(&heap));
            Assert::IsNotNull(ff_dx12_heap_cpu_data(&heap));
            Assert::AreNotEqual((D3D12_GPU_VIRTUAL_ADDRESS)0, ff_dx12_heap_gpu_data(&heap));
            Assert::IsTrue(ff_dx12_heap_cpu_usage(&heap));

            ff_dx12_heap_destroy(&heap);
        }

        TEST_METHOD(readback_heap_has_mapped_cpu_data)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_heap heap{};
            Assert::IsTrue(ff_dx12_heap_init(&heap, nullptr, FF_SVL("readback heap"), 64 * 1024, ff_dx12_heap_usage_readback));
            Assert::IsNotNull(ff_dx12_heap_cpu_data(&heap));
            Assert::IsTrue(ff_dx12_heap_cpu_usage(&heap));

            ff_dx12_heap_destroy(&heap);
        }

        TEST_METHOD(gpu_buffers_heap_has_no_cpu_data)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_heap heap{};
            Assert::IsTrue(ff_dx12_heap_init(&heap, nullptr, FF_SVL("gpu buffers heap"), 64 * 1024, ff_dx12_heap_usage_gpu_buffers));
            Assert::IsNull(ff_dx12_heap_cpu_data(&heap));
            Assert::AreEqual((D3D12_GPU_VIRTUAL_ADDRESS)0, ff_dx12_heap_gpu_data(&heap));
            Assert::IsFalse(ff_dx12_heap_cpu_usage(&heap));

            ff_dx12_heap_destroy(&heap);
        }

        TEST_METHOD(gpu_textures_heap_has_no_cpu_data)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_heap heap{};
            Assert::IsTrue(ff_dx12_heap_init(&heap, nullptr, FF_SVL("gpu textures heap"), 4 * 1024 * 1024, ff_dx12_heap_usage_gpu_textures));
            Assert::IsNull(ff_dx12_heap_cpu_data(&heap));

            ff_dx12_heap_destroy(&heap);
        }

        TEST_METHOD(gpu_targets_heap_has_no_cpu_data)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_heap heap{};
            Assert::IsTrue(ff_dx12_heap_init(&heap, nullptr, FF_SVL("gpu targets heap"), 4 * 1024 * 1024, ff_dx12_heap_usage_gpu_targets));
            Assert::IsNull(ff_dx12_heap_cpu_data(&heap));

            ff_dx12_heap_destroy(&heap);
        }

        TEST_METHOD(usage_name_matches_each_enum_value)
        {
            Assert::AreEqual("upload", std::string(ff_dx12_heap_usage_name(ff_dx12_heap_usage_upload).data,
                ff_dx12_heap_usage_name(ff_dx12_heap_usage_upload).count).c_str());
            Assert::AreEqual("readback", std::string(ff_dx12_heap_usage_name(ff_dx12_heap_usage_readback).data,
                ff_dx12_heap_usage_name(ff_dx12_heap_usage_readback).count).c_str());
            Assert::AreEqual("gpu_buffers", std::string(ff_dx12_heap_usage_name(ff_dx12_heap_usage_gpu_buffers).data,
                ff_dx12_heap_usage_name(ff_dx12_heap_usage_gpu_buffers).count).c_str());
            Assert::AreEqual("gpu_textures", std::string(ff_dx12_heap_usage_name(ff_dx12_heap_usage_gpu_textures).data,
                ff_dx12_heap_usage_name(ff_dx12_heap_usage_gpu_textures).count).c_str());
            Assert::AreEqual("gpu_targets", std::string(ff_dx12_heap_usage_name(ff_dx12_heap_usage_gpu_targets).data,
                ff_dx12_heap_usage_name(ff_dx12_heap_usage_gpu_targets).count).c_str());
        }

        TEST_METHOD(destroy_is_safe_when_not_initialized)
        {
            ff_dx12_heap heap{};
            ff_dx12_heap_destroy(&heap);
            Assert::IsFalse(ff_dx12_heap_valid(&heap));
        }

        TEST_METHOD(shared_heap_bookkeeping_does_not_own_its_arena)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));
            ff_arena_declare_stack(storage_arena, 1024);
            ff_dx12_heap heaps[2]{};
            for (auto& heap : heaps)
            {
                Assert::IsTrue(ff_dx12_heap_init(&heap, &storage_arena,
                    FF_SVL("shared heap"), 64 * 1024, ff_dx12_heap_usage_upload));
                Assert::IsNull(heap.arena.heap);
                Assert::IsTrue(heap.storage_arena == &storage_arena);
                Assert::IsTrue(heap.residency_data.keep_resident.arena == &storage_arena);
            }

            uint32_t* sentinel = ff_arena_alloc_type(&storage_arena, uint32_t, 1);
            *sentinel = 123;
            const ff_arena_marker marker = ff_arena_mark(&storage_arena);
            ff_dx12_heap_destroy(&heaps[0]);
            ff_dx12_heap_destroy(&heaps[0]);
            Assert::IsTrue(marker == ff_arena_mark(&storage_arena));
            Assert::AreEqual((uint32_t)123, *sentinel);
            Assert::IsTrue(ff_dx12_heap_valid(&heaps[1]));
            uint32_t* mapped = (uint32_t*)ff_dx12_heap_cpu_data(&heaps[1]);
            *mapped = 456;
            Assert::AreEqual((uint32_t)456, *mapped);
            ff_dx12_heap_destroy(&heaps[1]);
            ff_arena_destroy(&storage_arena);
        }

        TEST_METHOD(residency_data_pointer_is_non_null_after_init)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_heap heap{};
            Assert::IsTrue(ff_dx12_heap_init(&heap, nullptr, FF_SVL("residency heap"), 64 * 1024, ff_dx12_heap_usage_gpu_buffers));
            Assert::IsNotNull(ff_dx12_heap_residency_data(&heap));

            ff_dx12_heap_destroy(&heap);
        }
    };
}
