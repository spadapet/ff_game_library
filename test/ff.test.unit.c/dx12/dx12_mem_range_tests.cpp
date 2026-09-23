#include "pch.h"

namespace ff::test::dx12
{
    TEST_CLASS(dx12_mem_range_tests)
    {
    public:
        TEST_METHOD_CLEANUP(cleanup)
        {
            ff_dx12_destroy();
        }

        TEST_METHOD(zero_initialized_range_is_invalid)
        {
            ff_dx12_mem_range range{};
            Assert::IsFalse(ff_dx12_mem_range_valid(&range));
            Assert::IsNull(ff_dx12_mem_range_cpu_data(&range));
            Assert::AreEqual((D3D12_GPU_VIRTUAL_ADDRESS)0, ff_dx12_mem_range_gpu_data(&range));
            Assert::IsNull(ff_dx12_mem_range_heap(&range));

            // Freeing an already-invalid range must be safe (no owner call happens).
            ff_dx12_mem_range_free(&range);
        }

        TEST_METHOD(alloc_from_free_list_buffer_gives_valid_range)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_arena arena{};
            ff_arena_init_heap_local(&arena, 0);

            ff_dx12_mem_buffer buffer{};
            Assert::IsTrue(ff_dx12_mem_buffer_init_free_list(&buffer, &arena, FF_SVL("range test buffer"), 64 * 1024, ff_dx12_heap_usage_upload));

            ff_dx12_mem_range range = ff_dx12_mem_buffer_alloc_bytes(&buffer, 256, 16, ff_dx12_fence_value{});
            Assert::IsTrue(ff_dx12_mem_range_valid(&range));
            Assert::IsNotNull(ff_dx12_mem_range_cpu_data(&range));
            Assert::IsNotNull(ff_dx12_mem_range_heap(&range));
            Assert::IsNotNull(ff_dx12_mem_range_residency_data(&range));

            ff_dx12_mem_range_free(&range);
            Assert::IsFalse(ff_dx12_mem_range_valid(&range));

            ff_dx12_mem_buffer_destroy(&buffer);
            ff_arena_destroy(&arena);
        }
    };
}
