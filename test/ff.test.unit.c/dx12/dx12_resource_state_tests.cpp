#include "pch.h"

namespace ff::test::dx12
{
    TEST_CLASS(dx12_resource_state_tests)
    {
    public:
        TEST_METHOD(single_subresource_starts_all_same)
        {
            ff_dx12_resource_state states{};
            ff_dx12_resource_state_init(&states, nullptr, D3D12_RESOURCE_STATE_COMMON,
                ff_dx12_resource_state_type_global, 1, 1);

            Assert::AreEqual((size_t)1, ff_dx12_resource_state_sub_resource_size(&states));
            Assert::IsTrue(ff_dx12_resource_state_all_same(&states));

            ff_dx12_resource_state_entry entry = ff_dx12_resource_state_get(&states, 0, nullptr);
            Assert::AreEqual((int)D3D12_RESOURCE_STATE_COMMON, (int)entry.state);
            Assert::AreEqual((int)ff_dx12_resource_state_type_global, (int)entry.type);
        }

        TEST_METHOD(setting_every_subresource_collapses_to_one_entry)
        {
            ff_arena arena{};
            ff_arena_init_heap_local(&arena, 0);

            ff_dx12_resource_state states{};
            ff_dx12_resource_state_init(&states, &arena, D3D12_RESOURCE_STATE_COMMON,
                ff_dx12_resource_state_type_global, 2, 3);
            Assert::AreEqual((size_t)6, ff_dx12_resource_state_sub_resource_size(&states));

            ff_dx12_resource_state_set(&states, D3D12_RESOURCE_STATE_RENDER_TARGET,
                ff_dx12_resource_state_type_global, 0, 6);

            Assert::IsTrue(ff_dx12_resource_state_all_same(&states));
            Assert::AreEqual((size_t)1, states.count);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(setting_one_subresource_diverges_then_recollapses)
        {
            ff_arena arena{};
            ff_arena_init_heap_local(&arena, 0);

            ff_dx12_resource_state states{};
            ff_dx12_resource_state_init(&states, &arena, D3D12_RESOURCE_STATE_COMMON,
                ff_dx12_resource_state_type_global, 1, 4);

            ff_dx12_resource_state_set(&states, D3D12_RESOURCE_STATE_RENDER_TARGET,
                ff_dx12_resource_state_type_global, 2, 1);

            Assert::IsFalse(ff_dx12_resource_state_all_same(&states));
            Assert::AreEqual((int)D3D12_RESOURCE_STATE_RENDER_TARGET,
                (int)ff_dx12_resource_state_get(&states, 2, nullptr).state);
            Assert::AreEqual((int)D3D12_RESOURCE_STATE_COMMON,
                (int)ff_dx12_resource_state_get(&states, 0, nullptr).state);

            ff_dx12_resource_state_set(&states, D3D12_RESOURCE_STATE_RENDER_TARGET,
                ff_dx12_resource_state_type_global, 0, 4);
            Assert::IsTrue(ff_dx12_resource_state_all_same(&states));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(state_spills_into_the_arena_past_inline_capacity)
        {
            ff_arena arena{};
            ff_arena_init_heap_local(&arena, 0);

            const size_t mip_size = FF_DX12_RESOURCE_STATE_INLINE_MAX * 4;
            ff_dx12_resource_state states{};
            ff_dx12_resource_state_init(&states, &arena, D3D12_RESOURCE_STATE_COMMON,
                ff_dx12_resource_state_type_global, 1, mip_size);

            ff_dx12_resource_state_set(&states, D3D12_RESOURCE_STATE_COPY_DEST,
                ff_dx12_resource_state_type_global, mip_size - 1, 1);

            Assert::IsNotNull((void*)states.overflow);
            Assert::AreEqual(mip_size, states.count);
            Assert::AreEqual((int)D3D12_RESOURCE_STATE_COPY_DEST,
                (int)ff_dx12_resource_state_get(&states, mip_size - 1, nullptr).state);
            Assert::AreEqual((int)D3D12_RESOURCE_STATE_COMMON,
                (int)ff_dx12_resource_state_get(&states, 0, nullptr).state);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(set_array_covers_the_right_subresources)
        {
            ff_arena arena{};
            ff_arena_init_heap_local(&arena, 0);

            ff_dx12_resource_state states{};
            ff_dx12_resource_state_init(&states, &arena, D3D12_RESOURCE_STATE_COMMON,
                ff_dx12_resource_state_type_global, 3, 4);

            ff_dx12_resource_state_set_array(&states, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                ff_dx12_resource_state_type_global, 1, 1, 1, 2);

            // Array slice 1, mips 1 and 2 => subresources 5 and 6.
            Assert::AreEqual((int)D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                (int)ff_dx12_resource_state_get(&states, 5, nullptr).state);
            Assert::AreEqual((int)D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                (int)ff_dx12_resource_state_get(&states, 6, nullptr).state);
            Assert::AreEqual((int)D3D12_RESOURCE_STATE_COMMON,
                (int)ff_dx12_resource_state_get(&states, 4, nullptr).state);
            Assert::AreEqual((int)D3D12_RESOURCE_STATE_COMMON,
                (int)ff_dx12_resource_state_get(&states, 7, nullptr).state);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(get_falls_back_when_the_state_is_none)
        {
            ff_dx12_resource_state fallback{};
            ff_dx12_resource_state_init(&fallback, nullptr, D3D12_RESOURCE_STATE_COPY_SOURCE,
                ff_dx12_resource_state_type_global, 1, 1);

            ff_dx12_resource_state states{};
            ff_dx12_resource_state_init(&states, nullptr, D3D12_RESOURCE_STATE_COMMON,
                ff_dx12_resource_state_type_none, 1, 1);

            ff_dx12_resource_state_entry entry = ff_dx12_resource_state_get(&states, 0, &fallback);
            Assert::AreEqual((int)D3D12_RESOURCE_STATE_COPY_SOURCE, (int)entry.state);
            Assert::AreEqual((int)ff_dx12_resource_state_type_global, (int)entry.type);
        }

        TEST_METHOD(merge_keeps_global_type_but_takes_the_new_state)
        {
            ff_dx12_resource_state states{};
            ff_dx12_resource_state_init(&states, nullptr, D3D12_RESOURCE_STATE_COMMON,
                ff_dx12_resource_state_type_global, 1, 1);

            ff_dx12_resource_state other{};
            ff_dx12_resource_state_init(&other, nullptr, D3D12_RESOURCE_STATE_RENDER_TARGET,
                ff_dx12_resource_state_type_barrier, 1, 1);

            ff_dx12_resource_state_merge(&states, &other);

            ff_dx12_resource_state_entry entry = ff_dx12_resource_state_get(&states, 0, nullptr);
            Assert::AreEqual((int)D3D12_RESOURCE_STATE_RENDER_TARGET, (int)entry.state);
            Assert::AreEqual((int)ff_dx12_resource_state_type_global, (int)entry.type);
        }

        TEST_METHOD(merge_ignores_none_entries)
        {
            ff_dx12_resource_state states{};
            ff_dx12_resource_state_init(&states, nullptr, D3D12_RESOURCE_STATE_RENDER_TARGET,
                ff_dx12_resource_state_type_barrier, 1, 1);

            ff_dx12_resource_state other{};
            ff_dx12_resource_state_init(&other, nullptr, D3D12_RESOURCE_STATE_COMMON,
                ff_dx12_resource_state_type_none, 1, 1);

            ff_dx12_resource_state_merge(&states, &other);

            Assert::AreEqual((int)D3D12_RESOURCE_STATE_RENDER_TARGET,
                (int)ff_dx12_resource_state_get(&states, 0, nullptr).state);
        }

        TEST_METHOD(copy_survives_the_source_arena_going_away)
        {
            ff_arena source_arena{};
            ff_arena_init_heap_local(&source_arena, 0);

            ff_arena dest_arena{};
            ff_arena_init_heap_local(&dest_arena, 0);

            const size_t mip_size = FF_DX12_RESOURCE_STATE_INLINE_MAX * 2;
            ff_dx12_resource_state source{};
            ff_dx12_resource_state_init(&source, &source_arena, D3D12_RESOURCE_STATE_COMMON,
                ff_dx12_resource_state_type_global, 1, mip_size);
            ff_dx12_resource_state_set(&source, D3D12_RESOURCE_STATE_COPY_DEST,
                ff_dx12_resource_state_type_global, 3, 1);

            ff_dx12_resource_state dest{};
            ff_dx12_resource_state_copy(&dest, &dest_arena, &source);

            ff_arena_destroy(&source_arena);

            Assert::AreEqual((int)D3D12_RESOURCE_STATE_COPY_DEST,
                (int)ff_dx12_resource_state_get(&dest, 3, nullptr).state);
            Assert::AreEqual((int)D3D12_RESOURCE_STATE_COMMON,
                (int)ff_dx12_resource_state_get(&dest, 0, nullptr).state);

            ff_arena_destroy(&dest_arena);
        }
    };
}
