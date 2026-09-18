#include "pch.h"

static ff_span span_of(const void* data, size_t size)
{
    ff_span span;
    span.data = data;
    span.size = size;
    return span;
}

static ff_span span_of_sz(const char* sz)
{
    return span_of(sz, ::strlen(sz));
}

static bool span_equals(ff_span span, const char* expected)
{
    size_t expected_size = ::strlen(expected);
    if (span.size != expected_size)
    {
        return false;
    }

    return expected_size == 0 || ::memcmp(span.data, expected, expected_size) == 0;
}

static ff_string_view temp_path(char* buffer, size_t buffer_size, const char* name)
{
    char temp_dir[MAX_PATH];
    DWORD temp_len = ::GetTempPathA((DWORD)std::size(temp_dir), temp_dir);
    Assert::IsTrue(temp_len > 0 && temp_len < std::size(temp_dir));

    int count = ::_snprintf_s(buffer, buffer_size, _TRUNCATE, "%sff_stream_test_%s_%lu.bin", temp_dir, name, ::GetCurrentProcessId());
    Assert::IsTrue(count > 0);

    ff_string_view view;
    view.data = buffer;
    view.count = (size_t)count;
    return view;
}

static ff_string_view write_temp_file(char* buffer, size_t buffer_size, const char* name, const char* contents)
{
    ff_string_view path = temp_path(buffer, buffer_size, name);

    ff_stream stream;
    Assert::IsTrue(ff_stream_init_write_file(&stream, path));
    Assert::IsTrue(ff_stream_write(&stream, span_of_sz(contents)));
    ff_stream_destroy(&stream);

    return path;
}

static void delete_temp_file(ff_string_view path)
{
    ::DeleteFileA(path.data);
}

namespace ff::test::base
{
    TEST_CLASS(stream_tests)
    {
    public:
        TEST_METHOD(read_memory_all)
        {
            const char source[] = "hello world";

            ff_stream stream;
            ff_stream_init_read_memory(&stream, span_of(source, 11));
            Assert::AreEqual((int)ff_stream_type_read_memory, (int)stream.type);
            Assert::AreEqual((size_t)11, ff_stream_size(&stream));
            Assert::AreEqual((size_t)0, ff_stream_pos(&stream));

            ff_span data = ff_stream_read(&stream, nullptr, 11, 1);
            Assert::IsTrue(span_equals(data, "hello world"));
            Assert::AreEqual((size_t)11, ff_stream_pos(&stream));

            ff_stream_destroy(&stream);
            Assert::AreEqual((int)ff_stream_type_none, (int)stream.type);
        }

        TEST_METHOD(read_memory_returns_view_into_source)
        {
            const char source[] = "abcdef";

            ff_stream stream;
            ff_stream_init_read_memory(&stream, span_of(source, 6));
            ff_span data = ff_stream_read(&stream, nullptr, 3, 1);

            Assert::AreEqual((void*)source, (void*)data.data);
            ff_stream_destroy(&stream);
        }

        TEST_METHOD(read_memory_in_chunks)
        {
            const char source[] = "0123456789";

            ff_stream stream;
            ff_stream_init_read_memory(&stream, span_of(source, 10));

            Assert::IsTrue(span_equals(ff_stream_read(&stream, nullptr, 4, 1), "0123"));
            Assert::AreEqual((size_t)4, ff_stream_pos(&stream));

            Assert::IsTrue(span_equals(ff_stream_read(&stream, nullptr, 4, 1), "4567"));
            Assert::AreEqual((size_t)8, ff_stream_pos(&stream));

            Assert::IsTrue(span_equals(ff_stream_read(&stream, nullptr, 100, 1), "89"));
            Assert::AreEqual((size_t)10, ff_stream_pos(&stream));

            ff_span eof = ff_stream_read(&stream, nullptr, 1, 1);
            Assert::AreEqual((size_t)0, eof.size);
            Assert::AreEqual((size_t)10, ff_stream_pos(&stream));

            ff_stream_destroy(&stream);
        }

        TEST_METHOD(read_memory_aligned_stays_in_place)
        {
            alignas(64) char source[128];
            ::memcpy(source, "file contents", 13);

            ff_stream stream;
            ff_stream_init_read_memory(&stream, span_of(source, std::size(source)));

            ff_span data = ff_stream_read(&stream, nullptr, 13, 64);
            Assert::IsTrue(span_equals(data, "file contents"));
            Assert::IsTrue(data.data == source);

            ff_stream_destroy(&stream);
        }

        TEST_METHOD(read_memory_misaligned_copies_into_arena)
        {
            alignas(64) char buffer[128];
            ::memcpy(buffer + 1, "file contents", 13);

            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_stream stream;
            ff_stream_init_read_memory(&stream, span_of(buffer + 1, 13));

            ff_span data = ff_stream_read(&stream, &arena, 13, 64);
            Assert::IsTrue(span_equals(data, "file contents"));
            Assert::IsTrue(data.data != buffer + 1);
            Assert::AreEqual((size_t)0, (size_t)((uintptr_t)data.data & 63));
            Assert::AreEqual((size_t)13, ff_stream_pos(&stream));

            ff_stream_destroy(&stream);
            ff_arena_destroy(&arena);
        }

        TEST_METHOD(read_memory_empty)
        {
            ff_span empty;
            empty.data = nullptr;
            empty.size = 0;

            ff_stream stream;
            ff_stream_init_read_memory(&stream, empty);
            Assert::AreEqual((size_t)0, ff_stream_size(&stream));
            Assert::AreEqual((size_t)0, ff_stream_read(&stream, nullptr, 10, 1).size);

            ff_stream_destroy(&stream);
        }

        TEST_METHOD(read_memory_zero_size)
        {
            const char source[] = "data";

            ff_stream stream;
            ff_stream_init_read_memory(&stream, span_of(source, 4));
            Assert::AreEqual((size_t)0, ff_stream_read(&stream, nullptr, 0, 1).size);
            Assert::AreEqual((size_t)0, ff_stream_pos(&stream));

            ff_stream_destroy(&stream);
        }

        TEST_METHOD(read_memory_seek)
        {
            const char source[] = "0123456789";

            ff_stream stream;
            ff_stream_init_read_memory(&stream, span_of(source, 10));

            Assert::IsTrue(ff_stream_seek(&stream, 6));
            Assert::AreEqual((size_t)6, ff_stream_pos(&stream));
            Assert::IsTrue(span_equals(ff_stream_read(&stream, nullptr, 2, 1), "67"));

            Assert::IsTrue(ff_stream_seek(&stream, 0));
            Assert::IsTrue(span_equals(ff_stream_read(&stream, nullptr, 2, 1), "01"));

            Assert::IsTrue(ff_stream_seek(&stream, 1000));
            Assert::AreEqual((size_t)10, ff_stream_pos(&stream));
            Assert::AreEqual((size_t)0, ff_stream_read(&stream, nullptr, 1, 1).size);

            ff_stream_destroy(&stream);
        }
        TEST_METHOD(write_memory_basic)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_stream stream;
            ff_stream_init_write_memory(&stream, &arena, 0);
            Assert::AreEqual((void*)&arena, (void*)stream.arena);
            Assert::AreEqual((size_t)0, ff_stream_size(&stream));

            Assert::IsTrue(ff_stream_write(&stream, span_of_sz("hello")));
            Assert::AreEqual((size_t)5, ff_stream_size(&stream));
            Assert::AreEqual((size_t)5, ff_stream_pos(&stream));

            Assert::IsTrue(ff_stream_write(&stream, span_of_sz(" world")));
            Assert::AreEqual((size_t)11, ff_stream_size(&stream));

            ff_span result = ff_stream_written(&stream);
            ff_stream_destroy(&stream);
            Assert::IsTrue(span_equals(result, "hello world"));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(write_memory_initial_capacity)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_stream stream;
            ff_stream_init_write_memory(&stream, &arena, 256);
            Assert::IsTrue(stream.capacity >= 256);
            Assert::AreEqual((size_t)0, ff_stream_size(&stream));

            Assert::IsTrue(ff_stream_write(&stream, span_of_sz("abc")));
            Assert::IsTrue(span_equals(ff_stream_written(&stream), "abc"));
            ff_stream_destroy(&stream);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(write_memory_grows_past_initial_capacity)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_stream stream;
            ff_stream_init_write_memory(&stream, &arena, 1);

            const size_t chunk_count = 500;
            for (size_t i = 0; i < chunk_count; i++)
            {
                Assert::IsTrue(ff_stream_write(&stream, span_of_sz("0123456789")));
            }

            ff_span result = ff_stream_written(&stream);
            ff_stream_destroy(&stream);
            Assert::AreEqual(chunk_count * 10, result.size);

            const uint8_t* bytes = (const uint8_t*)result.data;
            for (size_t i = 0; i < result.size; i++)
            {
                Assert::AreEqual((uint8_t)('0' + (i % 10)), bytes[i]);
            }

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(write_memory_single_large_write)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 128);

            const size_t size = 100 * 1024;
            uint8_t* source = (uint8_t*)::malloc(size);
            Assert::IsNotNull(source);

            for (size_t i = 0; i < size; i++)
            {
                source[i] = (uint8_t)(i & 0xFF);
            }

            ff_stream stream;
            ff_stream_init_write_memory(&stream, &arena, 0);
            Assert::IsTrue(ff_stream_write(&stream, span_of(source, size)));

            ff_span result = ff_stream_written(&stream);
            ff_stream_destroy(&stream);
            Assert::AreEqual(size, result.size);
            Assert::AreEqual(0, ::memcmp(result.data, source, size));

            ::free(source);
            ff_arena_destroy(&arena);
        }

        TEST_METHOD(write_memory_empty_write)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_stream stream;
            ff_stream_init_write_memory(&stream, &arena, 0);

            ff_span empty;
            empty.data = nullptr;
            empty.size = 0;
            Assert::IsTrue(ff_stream_write(&stream, empty));
            Assert::AreEqual((size_t)0, ff_stream_size(&stream));

            ff_span result = ff_stream_written(&stream);
            ff_stream_destroy(&stream);
            Assert::AreEqual((size_t)0, result.size);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(write_memory_result_outlives_stream)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_stream stream;
            ff_stream_init_write_memory(&stream, &arena, 0);
            Assert::IsTrue(ff_stream_write(&stream, span_of_sz("persisted")));
            ff_span result = ff_stream_written(&stream);
            ff_stream_destroy(&stream);

            void* unrelated = ff_arena_alloc(&arena, 1024, 16);
            Assert::IsNotNull(unrelated);
            ::memset(unrelated, 0xCD, 1024);

            Assert::IsTrue(span_equals(result, "persisted"));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(write_memory_round_trips_through_read_memory)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_stream writer;
            ff_stream_init_write_memory(&writer, &arena, 0);
            Assert::IsTrue(ff_stream_write(&writer, span_of_sz("round trip")));
            ff_span written = ff_stream_written(&writer);
            ff_stream_destroy(&writer);

            ff_stream reader;
            ff_stream_init_read_memory(&reader, written);
            Assert::AreEqual((size_t)10, ff_stream_size(&reader));
            Assert::IsTrue(span_equals(ff_stream_read(&reader, nullptr, 10, 1), "round trip"));
            ff_stream_destroy(&reader);

            ff_arena_destroy(&arena);
        }
        TEST_METHOD(written_tracks_writes_before_destroy)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_stream stream;
            ff_stream_init_write_memory(&stream, &arena, 0);
            Assert::AreEqual((size_t)0, ff_stream_written(&stream).size);

            Assert::IsTrue(ff_stream_write(&stream, span_of_sz("abc")));
            Assert::IsTrue(span_equals(ff_stream_written(&stream), "abc"));

            Assert::IsTrue(ff_stream_write(&stream, span_of_sz("def")));
            Assert::IsTrue(span_equals(ff_stream_written(&stream), "abcdef"));

            ff_stream_destroy(&stream);
            ff_arena_destroy(&arena);
        }

        TEST_METHOD(file_round_trip)
        {
            char path_buffer[MAX_PATH];
            ff_string_view path = write_temp_file(path_buffer, std::size(path_buffer), "round_trip", "file contents");

            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_stream stream;
            Assert::IsTrue(ff_stream_init_read_file(&stream, path));
            Assert::AreEqual((size_t)13, ff_stream_size(&stream));
            Assert::AreEqual((size_t)0, ff_stream_pos(&stream));

            ff_span data = ff_stream_read(&stream, &arena, 13, 1);
            Assert::IsTrue(span_equals(data, "file contents"));
            Assert::AreEqual((size_t)13, ff_stream_pos(&stream));

            ff_stream_destroy(&stream);
            ff_arena_destroy(&arena);
            delete_temp_file(path);
        }

        TEST_METHOD(read_file_honors_requested_alignment)
        {
            char path_buffer[MAX_PATH];
            ff_string_view path = write_temp_file(path_buffer, std::size(path_buffer), "align", "file contents");

            for (size_t align = 1; align <= 64; align *= 2)
            {
                for (size_t pad = 0; pad < align; pad++)
                {
                    ff_arena arena;
                    ff_arena_init_heap_global(&arena, 4096);

                    if (pad)
                    {
                        ff_arena_alloc(&arena, pad, 1);
                    }

                    ff_stream stream;
                    Assert::IsTrue(ff_stream_init_read_file(&stream, path));

                    ff_span data = ff_stream_read(&stream, &arena, 13, align);
                    Assert::IsTrue(span_equals(data, "file contents"));
                    Assert::AreEqual((size_t)0, (size_t)((uintptr_t)data.data & (align - 1)));

                    ff_stream_destroy(&stream);
                    ff_arena_destroy(&arena);
                }
            }

            delete_temp_file(path);
        }

        // An idict read back from a file is mapped in place, so the read must land on an aligned
        // address no matter what the arena happened to allocate beforehand.
        TEST_METHOD(idict_survives_file_round_trip_at_any_arena_offset)
        {
            char path_buffer[MAX_PATH];
            ff_string_view path = temp_path(path_buffer, std::size(path_buffer), "idict_align");

            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source;
            ff_dict_init(&source, &arena);
            ff_value value = ff_value_new_int32(4242);
            ff_dict_set(&source, FF_SVL("volume"), &value);

            ff_idict dict;
            ff_idict_init(&dict, &arena, &source);
            ff_span saved = ff_idict_save(&dict, &arena);

            ff_stream writer;
            Assert::IsTrue(ff_stream_init_write_file(&writer, path));
            Assert::IsTrue(ff_stream_write(&writer, saved));
            ff_stream_destroy(&writer);

            for (size_t pad = 0; pad < FF_IDICT_MAX_ALIGN; pad++)
            {
                ff_arena load_arena;
                ff_arena_init_heap_global(&load_arena, 4096);

                if (pad)
                {
                    ff_arena_alloc(&load_arena, pad, 1);
                }

                ff_stream reader;
                Assert::IsTrue(ff_stream_init_read_file(&reader, path));

                ff_span read = ff_stream_read(&reader, &load_arena, ff_stream_size(&reader), FF_IDICT_MAX_ALIGN);

                ff_idict loaded;
                Assert::IsTrue(ff_idict_load(&loaded, read, true, true));

                const ff_ivalue* found = ff_idict_get(&loaded, FF_SVL("volume"));
                Assert::IsNotNull(found);
                Assert::AreEqual(4242, found->i32);

                ff_stream_destroy(&reader);
                ff_arena_destroy(&load_arena);
            }

            ff_arena_destroy(&arena);
            delete_temp_file(path);
        }

        // A memory stream must be just as usable as a file stream here, which means honoring the
        // requested alignment even when the caller's bytes sit at a misaligned address.
        TEST_METHOD(idict_survives_memory_round_trip_at_any_offset)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source;
            ff_dict_init(&source, &arena);
            ff_value value = ff_value_new_int32(4242);
            ff_dict_set(&source, FF_SVL("volume"), &value);

            ff_idict dict;
            ff_idict_init(&dict, &arena, &source);
            ff_span saved = ff_idict_save(&dict, &arena);

            for (size_t pad = 0; pad < FF_IDICT_MAX_ALIGN; pad++)
            {
                ff_arena load_arena;
                ff_arena_init_heap_global(&load_arena, 4096);

                uint8_t* shifted = (uint8_t*)ff_arena_alloc(&load_arena, saved.size + FF_IDICT_MAX_ALIGN, FF_IDICT_MAX_ALIGN) + pad;
                ::memcpy(shifted, saved.data, saved.size);

                ff_stream reader;
                ff_stream_init_read_memory(&reader, span_of(shifted, saved.size));

                ff_span read = ff_stream_read(&reader, &load_arena, saved.size, FF_IDICT_MAX_ALIGN);

                ff_idict loaded;
                Assert::IsTrue(ff_idict_load(&loaded, read, true, true));

                const ff_ivalue* found = ff_idict_get(&loaded, FF_SVL("volume"));
                Assert::IsNotNull(found);
                Assert::AreEqual(4242, found->i32);

                ff_stream_destroy(&reader);
                ff_arena_destroy(&load_arena);
            }

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(read_file_in_chunks)
        {
            char path_buffer[MAX_PATH];
            ff_string_view path = write_temp_file(path_buffer, std::size(path_buffer), "chunks", "0123456789");

            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_stream stream;
            Assert::IsTrue(ff_stream_init_read_file(&stream, path));
            Assert::IsTrue(span_equals(ff_stream_read(&stream, &arena, 4, 1), "0123"));
            Assert::IsTrue(span_equals(ff_stream_read(&stream, &arena, 4, 1), "4567"));

            Assert::IsTrue(span_equals(ff_stream_read(&stream, &arena, 100, 1), "89"));
            Assert::AreEqual((size_t)0, ff_stream_read(&stream, &arena, 1, 1).size);

            ff_stream_destroy(&stream);
            ff_arena_destroy(&arena);
            delete_temp_file(path);
        }

        TEST_METHOD(read_file_seek)
        {
            char path_buffer[MAX_PATH];
            ff_string_view path = write_temp_file(path_buffer, std::size(path_buffer), "seek", "0123456789");

            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_stream stream;
            Assert::IsTrue(ff_stream_init_read_file(&stream, path));

            Assert::IsTrue(ff_stream_seek(&stream, 5));
            Assert::AreEqual((size_t)5, ff_stream_pos(&stream));
            Assert::IsTrue(span_equals(ff_stream_read(&stream, &arena, 3, 1), "567"));

            Assert::IsTrue(ff_stream_seek(&stream, 0));
            Assert::IsTrue(span_equals(ff_stream_read(&stream, &arena, 3, 1), "012"));

            Assert::IsTrue(ff_stream_seek(&stream, 999));
            Assert::AreEqual((size_t)10, ff_stream_pos(&stream));
            Assert::AreEqual((size_t)0, ff_stream_read(&stream, &arena, 1, 1).size);

            ff_stream_destroy(&stream);
            ff_arena_destroy(&arena);
            delete_temp_file(path);
        }

        TEST_METHOD(read_empty_file)
        {
            char path_buffer[MAX_PATH];
            ff_string_view path = write_temp_file(path_buffer, std::size(path_buffer), "empty", "");

            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_stream stream;
            Assert::IsTrue(ff_stream_init_read_file(&stream, path));
            Assert::AreEqual((size_t)0, ff_stream_size(&stream));
            Assert::AreEqual((size_t)0, ff_stream_read(&stream, &arena, 10, 1).size);

            ff_stream_destroy(&stream);
            ff_arena_destroy(&arena);
            delete_temp_file(path);
        }

        TEST_METHOD(read_file_binary_data)
        {
            char path_buffer[MAX_PATH];
            ff_string_view path = temp_path(path_buffer, std::size(path_buffer), "binary");

            uint8_t source[256];
            for (size_t i = 0; i < std::size(source); i++)
            {
                source[i] = (uint8_t)i;
            }

            ff_stream writer;
            Assert::IsTrue(ff_stream_init_write_file(&writer, path));
            Assert::IsTrue(ff_stream_write(&writer, span_of(source, std::size(source))));
            ff_stream_destroy(&writer);

            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_stream reader;
            Assert::IsTrue(ff_stream_init_read_file(&reader, path));
            Assert::AreEqual(std::size(source), ff_stream_size(&reader));

            ff_span data = ff_stream_read(&reader, &arena, std::size(source), 1);
            Assert::AreEqual(std::size(source), data.size);
            Assert::AreEqual(0, ::memcmp(data.data, source, std::size(source)));

            ff_stream_destroy(&reader);
            ff_arena_destroy(&arena);
            delete_temp_file(path);
        }
        TEST_METHOD(write_file_multiple_writes)
        {
            char path_buffer[MAX_PATH];
            ff_string_view path = temp_path(path_buffer, std::size(path_buffer), "multi_write");

            ff_stream writer;
            Assert::IsTrue(ff_stream_init_write_file(&writer, path));
            Assert::IsTrue(ff_stream_write(&writer, span_of_sz("one ")));
            Assert::IsTrue(ff_stream_write(&writer, span_of_sz("two ")));
            Assert::IsTrue(ff_stream_write(&writer, span_of_sz("three")));
            Assert::AreEqual((size_t)13, ff_stream_size(&writer));
            Assert::AreEqual((size_t)13, ff_stream_pos(&writer));

            ff_stream_destroy(&writer);

            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_stream reader;
            Assert::IsTrue(ff_stream_init_read_file(&reader, path));
            Assert::IsTrue(span_equals(ff_stream_read(&reader, &arena, 13, 1), "one two three"));
            ff_stream_destroy(&reader);

            ff_arena_destroy(&arena);
            delete_temp_file(path);
        }

        TEST_METHOD(write_file_truncates_existing)
        {
            char path_buffer[MAX_PATH];
            ff_string_view path = write_temp_file(path_buffer, std::size(path_buffer), "truncate", "a long previous value");

            ff_stream writer;
            Assert::IsTrue(ff_stream_init_write_file(&writer, path));
            Assert::IsTrue(ff_stream_write(&writer, span_of_sz("short")));
            ff_stream_destroy(&writer);

            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_stream reader;
            Assert::IsTrue(ff_stream_init_read_file(&reader, path));
            Assert::AreEqual((size_t)5, ff_stream_size(&reader));
            Assert::IsTrue(span_equals(ff_stream_read(&reader, &arena, 100, 1), "short"));
            ff_stream_destroy(&reader);

            ff_arena_destroy(&arena);
            delete_temp_file(path);
        }

        TEST_METHOD(write_file_creates_missing_directories)
        {
            char temp_dir[MAX_PATH];
            DWORD temp_len = ::GetTempPathA((DWORD)std::size(temp_dir), temp_dir);
            Assert::IsTrue(temp_len > 0 && temp_len < std::size(temp_dir));

            char root_buffer[MAX_PATH];
            int root_count = ::_snprintf_s(root_buffer, std::size(root_buffer), _TRUNCATE, "%sff_stream_test_dirs_%lu", temp_dir, ::GetCurrentProcessId());
            Assert::IsTrue(root_count > 0);

            char path_buffer[MAX_PATH];
            int path_count = ::_snprintf_s(path_buffer, std::size(path_buffer), _TRUNCATE, "%s\\one\\two\\three\\nested.bin", root_buffer);
            Assert::IsTrue(path_count > 0);

            ff_string_view path;
            path.data = path_buffer;
            path.count = (size_t)path_count;

            ff_stream writer;
            Assert::IsTrue(ff_stream_init_write_file(&writer, path));
            Assert::IsTrue(ff_stream_write(&writer, span_of_sz("nested")));
            ff_stream_destroy(&writer);

            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_stream reader;
            Assert::IsTrue(ff_stream_init_read_file(&reader, path));
            Assert::IsTrue(span_equals(ff_stream_read(&reader, &arena, 100, 1), "nested"));
            ff_stream_destroy(&reader);

            ff_arena_destroy(&arena);

            ::DeleteFileA(path_buffer);
            ::_snprintf_s(path_buffer, std::size(path_buffer), _TRUNCATE, "%s\\one\\two\\three", root_buffer);
            ::RemoveDirectoryA(path_buffer);
            ::_snprintf_s(path_buffer, std::size(path_buffer), _TRUNCATE, "%s\\one\\two", root_buffer);
            ::RemoveDirectoryA(path_buffer);
            ::_snprintf_s(path_buffer, std::size(path_buffer), _TRUNCATE, "%s\\one", root_buffer);
            ::RemoveDirectoryA(path_buffer);
            ::RemoveDirectoryA(root_buffer);
        }

        TEST_METHOD(read_missing_file_fails)
        {
            char path_buffer[MAX_PATH];
            ff_string_view path = temp_path(path_buffer, std::size(path_buffer), "does_not_exist");
            ::DeleteFileA(path.data);

            ff_stream stream;
            Assert::IsFalse(ff_stream_init_read_file(&stream, path));
            Assert::AreEqual((int)ff_stream_type_none, (int)stream.type);
            ff_stream_destroy(&stream);
            Assert::AreEqual((int)ff_stream_type_none, (int)stream.type);
        }

        TEST_METHOD(read_invalid_path_fails)
        {
            ff_stream stream;
            Assert::IsFalse(ff_stream_init_read_file(&stream, FF_SVL("?:\\this\\path\\is\\not\\valid\\at\\all.bin")));
            Assert::AreEqual((int)ff_stream_type_none, (int)stream.type);
        }

        TEST_METHOD(destroy_accepts_a_zeroed_stream)
        {
            ff_stream stream{};
            ff_stream_destroy(&stream);

            Assert::AreEqual((int)ff_stream_type_none, (int)stream.type);
        }

        TEST_METHOD(destroy_is_idempotent)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_stream stream;
            ff_stream_init_write_memory(&stream, &arena, 0);
            Assert::IsTrue(ff_stream_write(&stream, span_of_sz("data")));
            Assert::IsTrue(span_equals(ff_stream_written(&stream), "data"));

            ff_stream_destroy(&stream);
            Assert::AreEqual((int)ff_stream_type_none, (int)stream.type);

            ff_stream_destroy(&stream);
            Assert::AreEqual((int)ff_stream_type_none, (int)stream.type);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(file_to_memory_round_trip)
        {
            char path_buffer[MAX_PATH];
            ff_string_view path = write_temp_file(path_buffer, std::size(path_buffer), "file_to_memory", "copy me");

            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_stream reader;
            ff_stream writer;
            Assert::IsTrue(ff_stream_init_read_file(&reader, path));
            ff_stream_init_write_memory(&writer, &arena, 0);

            for (ff_span chunk = ff_stream_read(&reader, &arena, 3, 1); chunk.size; chunk = ff_stream_read(&reader, &arena, 3, 1))
            {
                Assert::IsTrue(ff_stream_write(&writer, chunk));
            }

            ff_stream_destroy(&reader);
            Assert::IsTrue(span_equals(ff_stream_written(&writer), "copy me"));
            ff_stream_destroy(&writer);

            ff_arena_destroy(&arena);
            delete_temp_file(path);
        }
    };
}
