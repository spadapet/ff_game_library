#include "pch.h"

namespace ff::test::dx12
{
    TEST_CLASS(dx12_object_cache_tests)
    {
    public:
        TEST_METHOD_CLEANUP(cleanup)
        {
            ff_dx12_destroy();
        }

        static D3D12_VERSIONED_ROOT_SIGNATURE_DESC empty_root_signature_desc(D3D12_ROOT_SIGNATURE_FLAGS flags)
        {
            D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc{};
            desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
            desc.Desc_1_1.Flags = flags;
            return desc;
        }

        TEST_METHOD(same_root_signature_desc_returns_same_object)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_object_cache cache{};
            ff_dx12_object_cache_init(&cache);

            D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc = empty_root_signature_desc(D3D12_ROOT_SIGNATURE_FLAG_NONE);
            ID3D12RootSignature* a = ff_dx12_object_cache_root_signature(&cache, &desc);
            ID3D12RootSignature* b = ff_dx12_object_cache_root_signature(&cache, &desc);

            Assert::IsNotNull(a);
            Assert::IsTrue(a == b);

            ff_dx12_object_cache_destroy(&cache);
        }

        TEST_METHOD(different_root_signature_desc_returns_different_object)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_object_cache cache{};
            ff_dx12_object_cache_init(&cache);

            D3D12_VERSIONED_ROOT_SIGNATURE_DESC a_desc = empty_root_signature_desc(D3D12_ROOT_SIGNATURE_FLAG_NONE);
            D3D12_VERSIONED_ROOT_SIGNATURE_DESC b_desc = empty_root_signature_desc(D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

            ID3D12RootSignature* a = ff_dx12_object_cache_root_signature(&cache, &a_desc);
            ID3D12RootSignature* b = ff_dx12_object_cache_root_signature(&cache, &b_desc);

            Assert::IsNotNull(a);
            Assert::IsNotNull(b);
            Assert::IsTrue(a != b);

            ff_dx12_object_cache_destroy(&cache);
        }

        TEST_METHOD(root_signature_hash_round_trips)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_object_cache cache{};
            ff_dx12_object_cache_init(&cache);

            D3D12_VERSIONED_ROOT_SIGNATURE_DESC a_desc = empty_root_signature_desc(D3D12_ROOT_SIGNATURE_FLAG_NONE);
            D3D12_VERSIONED_ROOT_SIGNATURE_DESC b_desc = empty_root_signature_desc(D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

            ID3D12RootSignature* a = ff_dx12_object_cache_root_signature(&cache, &a_desc);
            ID3D12RootSignature* b = ff_dx12_object_cache_root_signature(&cache, &b_desc);

            const uint64_t a_hash = ff_dx12_object_cache_root_signature_hash(&cache, a);
            const uint64_t b_hash = ff_dx12_object_cache_root_signature_hash(&cache, b);

            Assert::IsTrue(a_hash != 0);
            Assert::IsTrue(b_hash != 0);
            Assert::IsTrue(a_hash != b_hash);
            Assert::AreEqual(a_hash, ff_dx12_object_cache_root_signature_hash(&cache, a));

            // An unknown root signature has no hash rather than colliding with a real one.
            Assert::AreEqual((uint64_t)0, ff_dx12_object_cache_root_signature_hash(&cache, nullptr));

            ff_dx12_object_cache_destroy(&cache);
        }

        // The cache is a hash table; many distinct entries must all stay retrievable.
        TEST_METHOD(many_root_signatures_all_remain_cached)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_object_cache cache{};
            ff_dx12_object_cache_init(&cache);

            const size_t count = 16;
            ID3D12RootSignature* created[count]{};

            for (size_t i = 0; i < count; i++)
            {
                D3D12_ROOT_PARAMETER1 param{};
                param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
                param.Constants.Num32BitValues = (UINT)(i + 1);

                D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc{};
                desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
                desc.Desc_1_1.NumParameters = 1;
                desc.Desc_1_1.pParameters = &param;

                created[i] = ff_dx12_object_cache_root_signature(&cache, &desc);
                Assert::IsNotNull(created[i]);
            }

            for (size_t i = 0; i < count; i++)
            {
                D3D12_ROOT_PARAMETER1 param{};
                param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
                param.Constants.Num32BitValues = (UINT)(i + 1);

                D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc{};
                desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
                desc.Desc_1_1.NumParameters = 1;
                desc.Desc_1_1.pParameters = &param;

                Assert::IsTrue(created[i] == ff_dx12_object_cache_root_signature(&cache, &desc));
            }

            ff_dx12_object_cache_destroy(&cache);
        }

        TEST_METHOD(destroy_is_idempotent_and_reusable)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_object_cache cache{};
            ff_dx12_object_cache_init(&cache);

            D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc = empty_root_signature_desc(D3D12_ROOT_SIGNATURE_FLAG_NONE);
            Assert::IsNotNull(ff_dx12_object_cache_root_signature(&cache, &desc));

            ff_dx12_object_cache_destroy(&cache);
            ff_dx12_object_cache_destroy(&cache);

            ff_dx12_object_cache_init(&cache);
            Assert::IsNotNull(ff_dx12_object_cache_root_signature(&cache, &desc));
            ff_dx12_object_cache_destroy(&cache);
        }

        TEST_METHOD(pipeline_state_hash_is_stable_and_desc_sensitive)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_object_cache cache{};
            ff_dx12_object_cache_init(&cache);

            D3D12_VERSIONED_ROOT_SIGNATURE_DESC root_desc = empty_root_signature_desc(
                D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
            ID3D12RootSignature* root_signature = ff_dx12_object_cache_root_signature(&cache, &root_desc);
            Assert::IsNotNull(root_signature);

            D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
            desc.pRootSignature = root_signature;
            desc.SampleMask = UINT_MAX;
            desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            desc.NumRenderTargets = 1;
            desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.SampleDesc.Count = 1;

            const uint64_t hash = ff_dx12_object_cache_pipeline_state_hash(&cache, &desc);
            Assert::AreEqual(hash, ff_dx12_object_cache_pipeline_state_hash(&cache, &desc));

            desc.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
            Assert::IsTrue(hash != ff_dx12_object_cache_pipeline_state_hash(&cache, &desc));

            // Formats past NumRenderTargets are not part of the identity of the pipeline.
            desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.RTVFormats[3] = DXGI_FORMAT_B8G8R8A8_UNORM;
            Assert::AreEqual(hash, ff_dx12_object_cache_pipeline_state_hash(&cache, &desc));

            ff_dx12_object_cache_destroy(&cache);
        }

        // The hash must follow the root signature's content, not its address, so that two caches
        // agree and a rebuilt-but-identical signature keeps the same pipeline identity.
        TEST_METHOD(pipeline_state_hash_follows_root_signature_content)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_VERSIONED_ROOT_SIGNATURE_DESC root_desc = empty_root_signature_desc(D3D12_ROOT_SIGNATURE_FLAG_NONE);

            ff_dx12_object_cache a{};
            ff_dx12_object_cache b{};
            ff_dx12_object_cache_init(&a);
            ff_dx12_object_cache_init(&b);

            ID3D12RootSignature* a_root = ff_dx12_object_cache_root_signature(&a, &root_desc);
            ID3D12RootSignature* b_root = ff_dx12_object_cache_root_signature(&b, &root_desc);
            Assert::IsNotNull(a_root);
            Assert::IsNotNull(b_root);

            D3D12_GRAPHICS_PIPELINE_STATE_DESC a_desc{};
            a_desc.pRootSignature = a_root;
            a_desc.SampleMask = UINT_MAX;
            a_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            a_desc.SampleDesc.Count = 1;

            D3D12_GRAPHICS_PIPELINE_STATE_DESC b_desc = a_desc;
            b_desc.pRootSignature = b_root;

            Assert::AreEqual(
                ff_dx12_object_cache_pipeline_state_hash(&a, &a_desc),
                ff_dx12_object_cache_pipeline_state_hash(&b, &b_desc));

            ff_dx12_object_cache_destroy(&a);
            ff_dx12_object_cache_destroy(&b);
        }

        // Root signatures are owned by the device, so a reset has to drop every cached object.
        // Holding one across a reset would hand callers a pointer to a dead device's object.
        TEST_METHOD(reset_empties_the_cache_and_it_refills)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_object_cache cache{};
            ff_dx12_object_cache_init(&cache);

            D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc = empty_root_signature_desc(D3D12_ROOT_SIGNATURE_FLAG_NONE);
            ID3D12RootSignature* before = ff_dx12_object_cache_root_signature(&cache, &desc);
            Assert::IsNotNull(before);

            // The cache knows this object before the reset and must not after it.
            Assert::AreEqual((size_t)1, ff_dx12_object_cache_size(&cache));

            Assert::IsTrue(ff_dx12_reset_device(true));

            Assert::AreEqual((size_t)0, ff_dx12_object_cache_size(&cache));

            // Refills against the new device rather than returning a stale entry.
            ID3D12RootSignature* after = ff_dx12_object_cache_root_signature(&cache, &desc);
            Assert::IsNotNull(after);
            Assert::IsTrue(after == ff_dx12_object_cache_root_signature(&cache, &desc));
            Assert::AreEqual((size_t)1, ff_dx12_object_cache_size(&cache));

            ff_dx12_object_cache_destroy(&cache);
        }

        // The entry structs are recycled through entries_free, so repeatedly emptying and
        // refilling the cache must not grow its arena.
        TEST_METHOD(repeated_resets_do_not_grow_the_cache_arena)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_object_cache cache{};
            ff_dx12_object_cache_init(&cache);

            auto fill = [&cache]()
            {
                for (size_t i = 0; i < 16; i++)
                {
                    D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc = empty_root_signature_desc(
                        (i & 1) ? D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
                                : D3D12_ROOT_SIGNATURE_FLAG_NONE);
                    desc.Desc_1_1.NumStaticSamplers = 0;
                    Assert::IsNotNull(ff_dx12_object_cache_root_signature(&cache, &desc));
                }
            };

            fill();
            Assert::IsTrue(ff_dx12_reset_device(true));
            Assert::AreEqual((size_t)0, ff_dx12_object_cache_size(&cache));
            fill();
            const ff_arena_marker after_first = ff_arena_mark(&cache.arena);
            const size_t filled_size = ff_dx12_object_cache_size(&cache);
            Assert::AreEqual((size_t)2, filled_size);

            for (size_t i = 0; i < 6; i++)
            {
                Assert::IsTrue(ff_dx12_reset_device(true));
                Assert::AreEqual((size_t)0, ff_dx12_object_cache_size(&cache));
                fill();
                Assert::AreEqual(filled_size, ff_dx12_object_cache_size(&cache));
            }

            Assert::IsTrue(after_first == ff_arena_mark(&cache.arena));

            ff_dx12_object_cache_destroy(&cache);
        }

        // A cache destroyed while a reset walk is in flight must unregister cleanly.
        TEST_METHOD(destroying_a_cache_after_a_reset_is_clean)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_object_cache cache{};
            ff_dx12_object_cache_init(&cache);

            D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc = empty_root_signature_desc(D3D12_ROOT_SIGNATURE_FLAG_NONE);
            Assert::IsNotNull(ff_dx12_object_cache_root_signature(&cache, &desc));

            Assert::IsTrue(ff_dx12_reset_device(true));

            // Destroy without refilling: the buckets are already empty, so this exercises the
            // double-release path that a reset plus a destroy could otherwise hit.
            ff_dx12_object_cache_destroy(&cache);
            ff_dx12_object_cache_destroy(&cache);
        }
    };
}
