#include "pch.h"
#include "res/test_resources.h"

static bool map_span_equals(ff_span span, const char* expected)
{
    size_t expected_size = ::strlen(expected);
    if (span.size != expected_size)
    {
        return false;
    }

    return expected_size == 0 || ::memcmp(span.data, expected, expected_size) == 0;
}

static ff_string_view map_temp_path(char* buffer, size_t buffer_size, const char* name)
{
    char temp_dir[MAX_PATH];
    DWORD temp_len = ::GetTempPathA((DWORD)std::size(temp_dir), temp_dir);
    Assert::IsTrue(temp_len > 0 && temp_len < std::size(temp_dir));

    int count = ::_snprintf_s(buffer, buffer_size, _TRUNCATE, "%sff_file_map_test_%s_%lu.bin", temp_dir, name, ::GetCurrentProcessId());
    Assert::IsTrue(count > 0);

    ff_string_view view;
    view.data = buffer;
    view.count = (size_t)count;
    return view;
}

static ff_string_view write_map_temp_file(char* buffer, size_t buffer_size, const char* name, const void* data, size_t size)
{
    ff_string_view path = map_temp_path(buffer, buffer_size, name);

    HANDLE file = ::CreateFileA(path.data, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    Assert::IsTrue(file != INVALID_HANDLE_VALUE);

    if (size)
    {
        DWORD written = 0;
        Assert::IsTrue(::WriteFile(file, data, (DWORD)size, &written, nullptr) != FALSE);
        Assert::AreEqual((DWORD)size, written);
    }

    ::CloseHandle(file);
    return path;
}

static ff_string_view write_map_temp_text(char* buffer, size_t buffer_size, const char* name, const char* contents)
{
    return write_map_temp_file(buffer, buffer_size, name, contents, ::strlen(contents));
}

static void delete_map_temp_file(ff_string_view path)
{
    ::DeleteFileA(path.data);
}

// Resources live in this test DLL, not the test runner exe, so the handle comes from an address
// inside this module rather than GetModuleHandle(NULL).
static HMODULE test_module()
{
    HMODULE module = nullptr;
    Assert::IsTrue(::GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCWSTR)&delete_map_temp_file,
        &module) != FALSE);
    return module;
}

namespace ff::test::base
{
    TEST_CLASS(file_map_tests)
    {
    public:
        TEST_METHOD(map_whole_file)
        {
            char path_buffer[MAX_PATH];
            ff_string_view path = write_map_temp_text(path_buffer, std::size(path_buffer), "whole", "hello world");

            ff_file_map map;
            Assert::IsTrue(ff_file_map_init(&map, path));
            Assert::AreEqual((size_t)11, ff_file_map_data(&map).size);
            Assert::IsTrue(map_span_equals(ff_file_map_data(&map), "hello world"));

            ff_file_map_destroy(&map);
            delete_map_temp_file(path);
        }

        TEST_METHOD(destroy_resets_state)
        {
            char path_buffer[MAX_PATH];
            ff_string_view path = write_map_temp_text(path_buffer, std::size(path_buffer), "reset", "data");

            ff_file_map map;
            Assert::IsTrue(ff_file_map_init(&map, path));
            ff_file_map_destroy(&map);

            Assert::AreEqual((size_t)0, ff_file_map_data(&map).size);
            Assert::IsNull(ff_file_map_data(&map).data);

            delete_map_temp_file(path);
        }

        TEST_METHOD(destroy_is_idempotent)
        {
            char path_buffer[MAX_PATH];
            ff_string_view path = write_map_temp_text(path_buffer, std::size(path_buffer), "idempotent", "data");

            ff_file_map map;
            Assert::IsTrue(ff_file_map_init(&map, path));
            ff_file_map_destroy(&map);
            ff_file_map_destroy(&map);

            delete_map_temp_file(path);
        }

        TEST_METHOD(destroy_accepts_a_zeroed_map)
        {
            ff_file_map map{};
            ff_file_map_destroy(&map);

            Assert::IsNull(ff_file_map_data(&map).data);
            Assert::AreEqual((size_t)0, ff_file_map_data(&map).size);
        }

        TEST_METHOD(map_empty_file)
        {
            char path_buffer[MAX_PATH];
            ff_string_view path = write_map_temp_text(path_buffer, std::size(path_buffer), "empty", "");

            ff_file_map map;
            Assert::IsTrue(ff_file_map_init(&map, path));
            Assert::AreEqual((size_t)0, ff_file_map_data(&map).size);
            Assert::IsNull(ff_file_map_data(&map).data);

            ff_file_map_destroy(&map);
            delete_map_temp_file(path);
        }

        TEST_METHOD(map_missing_file_fails)
        {
            char path_buffer[MAX_PATH];
            ff_string_view path = map_temp_path(path_buffer, std::size(path_buffer), "missing");
            ::DeleteFileA(path.data);

            ff_file_map map;
            Assert::IsFalse(ff_file_map_init(&map, path));
            ff_file_map_destroy(&map);
        }

        TEST_METHOD(map_data_is_page_aligned)
        {
            char path_buffer[MAX_PATH];
            ff_string_view path = write_map_temp_text(path_buffer, std::size(path_buffer), "aligned", "some resource bytes");

            ff_file_map map;
            Assert::IsTrue(ff_file_map_init(&map, path));

            ff_span data = ff_file_map_data(&map);
            Assert::AreEqual((size_t)0, (size_t)data.data % 64);

            ff_file_map_destroy(&map);
            delete_map_temp_file(path);
        }

        TEST_METHOD(map_binary_data_round_trip)
        {
            uint8_t source[512];
            for (size_t i = 0; i < std::size(source); i++)
            {
                source[i] = (uint8_t)(255 - (i % 256));
            }

            char path_buffer[MAX_PATH];
            ff_string_view path = write_map_temp_file(path_buffer, std::size(path_buffer), "binary", source, std::size(source));

            ff_file_map map;
            Assert::IsTrue(ff_file_map_init(&map, path));
            Assert::AreEqual(std::size(source), ff_file_map_data(&map).size);
            Assert::IsTrue(::memcmp(ff_file_map_data(&map).data, source, std::size(source)) == 0);

            ff_file_map_destroy(&map);
            delete_map_temp_file(path);
        }

        TEST_METHOD(map_allows_concurrent_readers)
        {
            char path_buffer[MAX_PATH];
            ff_string_view path = write_map_temp_text(path_buffer, std::size(path_buffer), "shared", "shared bytes");

            ff_file_map first;
            ff_file_map second;
            Assert::IsTrue(ff_file_map_init(&first, path));
            Assert::IsTrue(ff_file_map_init(&second, path));

            Assert::IsTrue(map_span_equals(ff_file_map_data(&first), "shared bytes"));
            Assert::IsTrue(map_span_equals(ff_file_map_data(&second), "shared bytes"));

            ff_file_map_destroy(&first);
            ff_file_map_destroy(&second);
            delete_map_temp_file(path);
        }

        TEST_METHOD(map_idict_round_trip)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict;
            ff_dict_init(&dict, &arena);

            ff_value name_value = ff_value_new_string(FF_SVL("resource"));
            ff_dict_set(&dict, FF_SVL("name"), &name_value);

            ff_value count_value = ff_value_new_int32(42);
            ff_dict_set(&dict, FF_SVL("count"), &count_value);

            ff_idict idict;
            ff_idict_init(&idict, &arena, &dict);

            ff_span saved = ff_idict_save(&idict, &arena);
            Assert::IsTrue(saved.size > 0);

            char path_buffer[MAX_PATH];
            ff_string_view path = write_map_temp_file(path_buffer, std::size(path_buffer), "idict", saved.data, saved.size);

            ff_file_map map;
            Assert::IsTrue(ff_file_map_init(&map, path));

            ff_idict loaded;
            Assert::IsTrue(ff_idict_load(&loaded, ff_file_map_data(&map), true, true));

            const ff_ivalue* name = ff_idict_get(&loaded, FF_SVL("name"));
            Assert::IsNotNull(name);
            ff_string_view loaded_name = ff_ivalue_as_string(name, &loaded);
            Assert::IsTrue(loaded_name.count == 8 && ::memcmp(loaded_name.data, "resource", 8) == 0);

            const ff_ivalue* count = ff_idict_get(&loaded, FF_SVL("count"));
            Assert::IsNotNull(count);
            Assert::AreEqual(42, count->i32);

            ff_file_map_destroy(&map);
            ff_arena_destroy(&arena);
            delete_map_temp_file(path);
        }
        TEST_METHOD(map_resource_by_integer_id)
        {
            ff_span data = ff_map_resource(test_module(), FF_RESOURCE_INT(FF_TEST_RES_DATA), FF_RESOURCE_INT(10));
            Assert::AreEqual((size_t)64, data.size);
            Assert::IsNotNull(data.data);

            const uint8_t* bytes = (const uint8_t*)data.data;
            for (size_t i = 0; i < data.size; i++)
            {
                Assert::AreEqual((uint8_t)((i * 7 + 3) % 256), bytes[i]);
            }
        }

        TEST_METHOD(map_resource_by_string_name)
        {
            ff_span data = ff_map_resource(test_module(), FF_WSVL(L"FF_TEST_RES_NAMED"), FF_RESOURCE_INT(10));
            Assert::IsTrue(map_span_equals(data, "resource text payload"));
        }

        TEST_METHOD(map_resource_by_name_that_is_not_null_terminated)
        {
            // 17 is the name length inside a longer buffer that has no terminator after it.
            const wchar_t buffer[] = L"FF_TEST_RES_NAMEDxxxJUNK";

            ff_wstring_view name;
            name.data = buffer;
            name.count = 17;

            ff_span data = ff_map_resource(test_module(), name, FF_RESOURCE_INT(10));
            Assert::IsTrue(map_span_equals(data, "resource text payload"));
        }

        TEST_METHOD(map_resource_name_respects_the_count)
        {
            // 20 runs past the name, so a lookup that honors the count cannot match.
            const wchar_t buffer[] = L"FF_TEST_RES_NAMEDxxx";

            ff_wstring_view name;
            name.data = buffer;
            name.count = 20;

            ff_span data = ff_map_resource(test_module(), name, FF_RESOURCE_INT(10));
            Assert::IsNull(data.data);
            Assert::AreEqual((size_t)0, data.size);
        }

        TEST_METHOD(map_resource_is_stable_across_calls)
        {
            ff_span first = ff_map_resource(test_module(), FF_RESOURCE_INT(FF_TEST_RES_TEXT), FF_RESOURCE_INT(10));
            ff_span second = ff_map_resource(test_module(), FF_RESOURCE_INT(FF_TEST_RES_TEXT), FF_RESOURCE_INT(10));

            Assert::IsTrue(first.data == second.data);
            Assert::AreEqual(first.size, second.size);
        }

        TEST_METHOD(map_missing_resource_returns_empty)
        {
            ff_span data = ff_map_resource(test_module(), FF_RESOURCE_INT(9999), FF_RESOURCE_INT(10));
            Assert::IsNull(data.data);
            Assert::AreEqual((size_t)0, data.size);
        }

        TEST_METHOD(map_missing_resource_name_returns_empty)
        {
            ff_span data = ff_map_resource(test_module(), FF_WSVL(L"NOT_A_RESOURCE"), FF_RESOURCE_INT(10));
            Assert::IsNull(data.data);
            Assert::AreEqual((size_t)0, data.size);
        }

        TEST_METHOD(map_resource_idict_round_trip)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict;
            ff_dict_init(&dict, &arena);

            ff_value text = ff_value_new_string(FF_SVL("from resource"));
            ff_dict_set(&dict, FF_SVL("name"), &text);

            ff_idict idict;
            ff_idict_init(&idict, &arena, &dict);
            ff_span saved = ff_idict_save(&idict, &arena);

            // A resource only gives 8 byte alignment, so prove a saved dict still loads from one.
            uint8_t* bytes = (uint8_t*)::_aligned_malloc(saved.size + 8, 64);
            Assert::IsNotNull(bytes);
            uint8_t* shifted = bytes + 8;
            ::memcpy(shifted, saved.data, saved.size);

            ff_span span;
            span.data = shifted;
            span.size = saved.size;

            ff_idict loaded;
            Assert::IsTrue(ff_idict_load(&loaded, span, true, true));

            ff_string_view name = ff_ivalue_as_string(ff_idict_get(&loaded, FF_SVL("name")), &loaded);
            Assert::IsTrue(name.count == 13 && ::memcmp(name.data, "from resource", 13) == 0);

            ::_aligned_free(bytes);
            ff_arena_destroy(&arena);
        }
    };
}