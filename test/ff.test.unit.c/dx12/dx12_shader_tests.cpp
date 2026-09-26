#include "pch.h"

// D3DReflect is used only to validate shader blobs in tests; the library itself never reflects.
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

namespace ff::test::dx12
{
    // The module-wide assert listener fails any test that trips an assert, which makes the
    // FF_ASSERT_RET_VAL guards unreachable from a test. Swapping in a counting listener for the
    // duration of one test lets a guard be verified instead of merely trusted.
    struct scoped_shader_assert_counter
    {
        scoped_shader_assert_counter()
        {
            scoped_shader_assert_counter::count = 0;
            this->previous = ff_assert_listener(&scoped_shader_assert_counter::handler);
        }

        ~scoped_shader_assert_counter()
        {
            ff_assert_listener(this->previous);
        }

        static bool handler(const char*, const char*, const char*, unsigned int)
        {
            scoped_shader_assert_counter::count++;
            return true;
        }

        static inline int count = 0;
        ff_assert_listener_func previous = nullptr;

        // Asserts compile out of Release, so the guard still takes its early-out path there but
        // no listener runs. Only the count differs between configurations.
        static int expected_count(int debug_count)
        {
#ifdef _DEBUG
            return debug_count;
#else
            return 0;
#endif
        }
    };

    // Shader delivery covers two things: the name table that maps the enum to a .cso file, and the
    // loader that memory maps those files out of the shaders directory next to the binary. The
    // blobs themselves are opaque, so what is checked here is that every shader resolves to real
    // DXBC, that the mapping is stable and shared, and that it outlives a device reset.
    TEST_CLASS(dx12_shader_tests)
    {
    public:
        TEST_METHOD_CLEANUP(cleanup)
        {
            ff_dx12_destroy();
        }

        TEST_METHOD(every_shader_has_a_name)
        {
            // The name table is indexed by enum value, so a shader added without a name would
            // otherwise read past the end of the table.
            for (int i = 0; i < ff_dx12_shader_count; i++)
            {
                const ff_string_view name = ff_dx12_shader_name((ff_dx12_shader)i);

                Assert::IsNotNull(name.data);
                Assert::AreNotEqual<size_t>(0, name.count);
                Assert::AreEqual<size_t>(::strlen(name.data), name.count);
            }
        }

        TEST_METHOD(shader_names_are_distinct)
        {
            // The name is also the .cso file name, so a duplicate would silently make two enum
            // values load the same shader.
            for (int i = 0; i < ff_dx12_shader_count; i++)
            {
                for (int j = i + 1; j < ff_dx12_shader_count; j++)
                {
                    const ff_string_view a = ff_dx12_shader_name((ff_dx12_shader)i);
                    const ff_string_view b = ff_dx12_shader_name((ff_dx12_shader)j);

                    Assert::IsFalse(ff_string_equal(a, b));
                }
            }
        }

        TEST_METHOD(every_shader_loads)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_object_cache cache{};
            ff_dx12_object_cache_init(&cache);

            for (int i = 0; i < ff_dx12_shader_count; i++)
            {
                const D3D12_SHADER_BYTECODE bytecode = ff_dx12_object_cache_shader(&cache, (ff_dx12_shader)i);

                Assert::IsNotNull(bytecode.pShaderBytecode);
                Assert::AreNotEqual<size_t>(0, bytecode.BytecodeLength);

                // Compiled shader containers start with 'DXBC'. This is what catches a build that
                // copied the .hlsl source, or truncated the file, rather than the compiled blob.
                const uint8_t* bytes = (const uint8_t*)bytecode.pShaderBytecode;
                Assert::AreEqual<uint8_t>('D', bytes[0]);
                Assert::AreEqual<uint8_t>('X', bytes[1]);
                Assert::AreEqual<uint8_t>('B', bytes[2]);
                Assert::AreEqual<uint8_t>('C', bytes[3]);
            }

            Assert::AreEqual((size_t)ff_dx12_shader_count, ff_dx12_object_cache_shader_count(&cache));

            ff_dx12_object_cache_destroy(&cache);
        }

        TEST_METHOD(shaders_are_distinct_blobs)
        {
            // ps_sprite.hlsl compiles four entry points and ps_color.hlsl two, so a build that
            // dropped the entry point would produce several identical .cso files.
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_object_cache cache{};
            ff_dx12_object_cache_init(&cache);

            for (int i = 0; i < ff_dx12_shader_count; i++)
            {
                const D3D12_SHADER_BYTECODE a = ff_dx12_object_cache_shader(&cache, (ff_dx12_shader)i);

                for (int j = i + 1; j < ff_dx12_shader_count; j++)
                {
                    const D3D12_SHADER_BYTECODE b = ff_dx12_object_cache_shader(&cache, (ff_dx12_shader)j);
                    const bool same = a.BytecodeLength == b.BytecodeLength &&
                        !::memcmp(a.pShaderBytecode, b.pShaderBytecode, a.BytecodeLength);

                    Assert::IsFalse(same);
                }
            }

            ff_dx12_object_cache_destroy(&cache);
        }

        TEST_METHOD(the_same_shader_returns_the_same_mapping)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_object_cache cache{};
            ff_dx12_object_cache_init(&cache);

            const D3D12_SHADER_BYTECODE a = ff_dx12_object_cache_shader(&cache, ff_dx12_shader_vs_sprite);
            const D3D12_SHADER_BYTECODE b = ff_dx12_object_cache_shader(&cache, ff_dx12_shader_vs_sprite);

            // Same pointer, not just equal bytes: asking twice must not map the file twice.
            Assert::IsTrue(a.pShaderBytecode == b.pShaderBytecode);
            Assert::AreEqual(a.BytecodeLength, b.BytecodeLength);
            Assert::AreEqual((size_t)1, ff_dx12_object_cache_shader_count(&cache));

            ff_dx12_object_cache_destroy(&cache);
        }

        // Shader blobs are file bytes with no device affinity. Dropping them on reset would be
        // pure waste, and worse, would invalidate any D3D12_SHADER_BYTECODE a caller still holds.
        TEST_METHOD(reset_keeps_shaders_while_emptying_device_objects)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_object_cache cache{};
            ff_dx12_object_cache_init(&cache);

            D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc{};
            desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
            Assert::IsNotNull(ff_dx12_object_cache_root_signature(&cache, &desc));
            Assert::AreEqual((size_t)1, ff_dx12_object_cache_size(&cache));

            const D3D12_SHADER_BYTECODE before = ff_dx12_object_cache_shader(&cache, ff_dx12_shader_ps_sprite);
            Assert::IsNotNull(before.pShaderBytecode);

            Assert::IsTrue(ff_dx12_reset_device(true));

            // The device objects are gone, but the shader mapping is untouched.
            Assert::AreEqual((size_t)0, ff_dx12_object_cache_size(&cache));
            Assert::AreEqual((size_t)1, ff_dx12_object_cache_shader_count(&cache));

            const D3D12_SHADER_BYTECODE after = ff_dx12_object_cache_shader(&cache, ff_dx12_shader_ps_sprite);
            Assert::IsTrue(before.pShaderBytecode == after.pShaderBytecode);
            Assert::AreEqual(before.BytecodeLength, after.BytecodeLength);

            ff_dx12_object_cache_destroy(&cache);
        }

        TEST_METHOD(shaders_are_compiled_for_the_stage_their_name_implies)
        {
            // Nothing else here would catch a .cso built with the wrong /T profile: a pixel shader
            // blob is a perfectly valid container, it just fails much later when a pipeline state
            // is built from it. The reflected program type is checked against the name prefix,
            // which is what the draw device relies on when it picks a shader for a stage.
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_object_cache cache{};
            ff_dx12_object_cache_init(&cache);

            for (int i = 0; i < ff_dx12_shader_count; i++)
            {
                const ff_string_view name = ff_dx12_shader_name((ff_dx12_shader)i);
                const D3D12_SHADER_BYTECODE bytecode = ff_dx12_object_cache_shader(&cache, (ff_dx12_shader)i);
                Assert::IsNotNull(bytecode.pShaderBytecode);

                ID3D12ShaderReflection* reflection = nullptr;
                Assert::IsTrue(SUCCEEDED(::D3DReflect(
                    bytecode.pShaderBytecode, bytecode.BytecodeLength, IID_PPV_ARGS(&reflection))));

                D3D12_SHADER_DESC desc{};
                Assert::IsTrue(SUCCEEDED(reflection->GetDesc(&desc)));
                reflection->Release();

                const bool is_vertex = !::strncmp(name.data, "vs_", 3);
                const D3D12_SHADER_VERSION_TYPE expected = is_vertex
                    ? D3D12_SHVER_VERTEX_SHADER
                    : D3D12_SHVER_PIXEL_SHADER;

                Assert::AreEqual((int)expected, (int)D3D12_SHVER_GET_TYPE(desc.Version));
            }

            ff_dx12_object_cache_destroy(&cache);
        }

        // Every blob is handed out as a raw pointer into a mapping the cache owns, so anything
        // that could move or free that mapping while a caller still holds bytecode is a
        // use-after-free. These cover the ways that could happen.

        TEST_METHOD(mapping_survives_unrelated_cache_growth)
        {
            // Entry structs come from the cache's arena, which relocates its blocks as it grows.
            // A blob that lived in that arena would move underneath a caller; it must not.
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_object_cache cache{};
            ff_dx12_object_cache_init(&cache);

            const D3D12_SHADER_BYTECODE before = ff_dx12_object_cache_shader(&cache, ff_dx12_shader_vs_sprite);
            Assert::IsNotNull(before.pShaderBytecode);

            const uint8_t first_byte = ((const uint8_t*)before.pShaderBytecode)[0];

            for (size_t i = 0; i < 256; i++)
            {
                D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc{};
                desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
                desc.Desc_1_1.Flags = (D3D12_ROOT_SIGNATURE_FLAGS)(i & 0x7f);
                ff_dx12_object_cache_root_signature(&cache, &desc);
            }

            const D3D12_SHADER_BYTECODE after = ff_dx12_object_cache_shader(&cache, ff_dx12_shader_vs_sprite);

            Assert::IsTrue(before.pShaderBytecode == after.pShaderBytecode);
            Assert::AreEqual(before.BytecodeLength, after.BytecodeLength);
            Assert::AreEqual(first_byte, ((const uint8_t*)before.pShaderBytecode)[0]);

            ff_dx12_object_cache_destroy(&cache);
        }

        TEST_METHOD(loading_every_shader_does_not_disturb_earlier_blobs)
        {
            // The maps live in a fixed array, so a later load must not touch an earlier one. A
            // full copy of each blob is kept and re-compared after all 11 are mapped, which would
            // catch an index or aliasing mistake that a pointer check alone would miss.
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_object_cache cache{};
            ff_dx12_object_cache_init(&cache);

            D3D12_SHADER_BYTECODE seen[ff_dx12_shader_count]{};
            static uint8_t copies[ff_dx12_shader_count][64 * 1024];

            for (int i = 0; i < ff_dx12_shader_count; i++)
            {
                seen[i] = ff_dx12_object_cache_shader(&cache, (ff_dx12_shader)i);
                Assert::IsNotNull(seen[i].pShaderBytecode);
                Assert::IsTrue(seen[i].BytecodeLength <= sizeof(copies[i]));

                ::memcpy(copies[i], seen[i].pShaderBytecode, seen[i].BytecodeLength);
            }

            for (int i = 0; i < ff_dx12_shader_count; i++)
            {
                const D3D12_SHADER_BYTECODE again = ff_dx12_object_cache_shader(&cache, (ff_dx12_shader)i);

                Assert::IsTrue(seen[i].pShaderBytecode == again.pShaderBytecode);
                Assert::AreEqual(seen[i].BytecodeLength, again.BytecodeLength);
                Assert::IsTrue(!::memcmp(copies[i], again.pShaderBytecode, again.BytecodeLength));
            }

            ff_dx12_object_cache_destroy(&cache);
        }

        TEST_METHOD(two_caches_map_the_same_file_independently)
        {
            // Each cache owns its own mapping, so destroying one must not unmap the other's view
            // of the same .cso. The file is opened FILE_SHARE_READ for exactly this reason.
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_object_cache first{};
            ff_dx12_object_cache second{};
            ff_dx12_object_cache_init(&first);
            ff_dx12_object_cache_init(&second);

            const D3D12_SHADER_BYTECODE a = ff_dx12_object_cache_shader(&first, ff_dx12_shader_ps_sprite);
            const D3D12_SHADER_BYTECODE b = ff_dx12_object_cache_shader(&second, ff_dx12_shader_ps_sprite);

            Assert::IsNotNull(a.pShaderBytecode);
            Assert::IsNotNull(b.pShaderBytecode);
            Assert::AreEqual(a.BytecodeLength, b.BytecodeLength);

            static uint8_t copy[64 * 1024];
            Assert::IsTrue(b.BytecodeLength <= sizeof(copy));
            ::memcpy(copy, b.pShaderBytecode, b.BytecodeLength);

            ff_dx12_object_cache_destroy(&first);

            // Reading through the surviving cache's pointer after the other was destroyed.
            Assert::IsTrue(!::memcmp(copy, b.pShaderBytecode, b.BytecodeLength));

            ff_dx12_object_cache_destroy(&second);
        }

        TEST_METHOD(destroy_unmaps_and_a_reinitialized_cache_reloads)
        {
            // destroy must release the mappings rather than leak them, and must leave the struct
            // inert enough that the same storage can be initialized and used again.
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_object_cache cache{};
            ff_dx12_object_cache_init(&cache);
            Assert::IsNotNull(ff_dx12_object_cache_shader(&cache, ff_dx12_shader_vs_line).pShaderBytecode);
            Assert::AreEqual((size_t)1, ff_dx12_object_cache_shader_count(&cache));

            ff_dx12_object_cache_destroy(&cache);
            Assert::AreEqual((size_t)0, ff_dx12_object_cache_shader_count(&cache));

            ff_dx12_object_cache_init(&cache);
            Assert::AreEqual((size_t)0, ff_dx12_object_cache_shader_count(&cache));

            const D3D12_SHADER_BYTECODE again = ff_dx12_object_cache_shader(&cache, ff_dx12_shader_vs_line);
            Assert::IsNotNull(again.pShaderBytecode);
            Assert::AreEqual((size_t)1, ff_dx12_object_cache_shader_count(&cache));

            ff_dx12_object_cache_destroy(&cache);
        }

        TEST_METHOD(double_destroy_does_not_double_unmap)
        {
            // ff_file_map_destroy zeroes the map, so the second pass must find nothing to release.
            // Without that, this would unmap a view and close handles twice.
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_object_cache cache{};
            ff_dx12_object_cache_init(&cache);
            Assert::IsNotNull(ff_dx12_object_cache_shader(&cache, ff_dx12_shader_ps_color).pShaderBytecode);

            ff_dx12_object_cache_destroy(&cache);
            ff_dx12_object_cache_destroy(&cache);

            Assert::AreEqual((size_t)0, ff_dx12_object_cache_shader_count(&cache));
        }

        TEST_METHOD(repeated_load_and_destroy_cycles_do_not_leak_handles)
        {
            // A mapping leaked per cycle would show up as a steadily climbing handle count. The
            // check is for growth across cycles, not an exact number, since the D3D12 runtime
            // moves its own handle count around underneath this.
            Assert::IsTrue(ff_dx12_init(nullptr));

            auto cycle = []()
            {
                ff_dx12_object_cache cache{};
                ff_dx12_object_cache_init(&cache);

                for (int i = 0; i < ff_dx12_shader_count; i++)
                {
                    Assert::IsNotNull(ff_dx12_object_cache_shader(&cache, (ff_dx12_shader)i).pShaderBytecode);
                }

                ff_dx12_object_cache_destroy(&cache);
            };

            cycle();

            DWORD before = 0;
            Assert::IsTrue(::GetProcessHandleCount(::GetCurrentProcess(), &before) != FALSE);

            for (int i = 0; i < 8; i++)
            {
                cycle();
            }

            DWORD after = 0;
            Assert::IsTrue(::GetProcessHandleCount(::GetCurrentProcess(), &after) != FALSE);

            // Each cycle opens 11 files and 11 mappings, so a leak would add at least 176 handles
            // over 8 cycles. A small drift from unrelated runtime activity is expected.
            Assert::IsTrue(after < before + 64);
        }

        TEST_METHOD(a_failed_load_stays_empty_and_is_not_retried)
        {
            // The out-of-range path returns empty bytecode rather than reading past the maps
            // array. A caller that ignored the result would otherwise pass a wild pointer to D3D.
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_object_cache cache{};
            ff_dx12_object_cache_init(&cache);

            scoped_shader_assert_counter counter;
            const D3D12_SHADER_BYTECODE bad = ff_dx12_object_cache_shader(&cache, (ff_dx12_shader)ff_dx12_shader_count);
            Assert::AreEqual(scoped_shader_assert_counter::expected_count(1), scoped_shader_assert_counter::count);

            Assert::IsNull(bad.pShaderBytecode);
            Assert::AreEqual((size_t)0, (size_t)bad.BytecodeLength);
            Assert::AreEqual((size_t)0, ff_dx12_object_cache_shader_count(&cache));

            ff_dx12_object_cache_destroy(&cache);
        }
    };
}
