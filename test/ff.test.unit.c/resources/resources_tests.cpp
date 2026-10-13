#include "pch.h"

namespace ff::test::resources
{
    struct scoped_arena
    {
        ff_arena value{};

        scoped_arena()
        {
            ff_arena_init_heap_local(&this->value, 0);
        }

        ~scoped_arena()
        {
            ff_arena_destroy(&this->value);
        }
    };

    struct scoped_cache
    {
        ff_resource_cache value{};

        ~scoped_cache()
        {
            ff_resource_cache_destroy(&this->value);
        }
    };

    struct scoped_module
    {
        ff_resource_module value{};

        ~scoped_module()
        {
            ff_resource_module_destroy(&this->value);
        }
    };

    struct scoped_registry
    {
        ff_resources value{};

        scoped_registry()
        {
            ff_resources_init(&this->value);
        }

        ~scoped_registry()
        {
            ff_resources_destroy(&this->value);
        }
    };

    struct scoped_stream
    {
        ff_stream value{};

        ~scoped_stream()
        {
            ff_stream_destroy(&this->value);
        }
    };

    struct scoped_global_resources
    {
        bool initialized{};

        ~scoped_global_resources()
        {
            if (this->initialized)
            {
                ff_resources_global_destroy();
            }
        }
    };

    static ff_span make_span(const void* data, size_t size)
    {
        ff_span result{};
        result.data = data;
        result.size = size;
        return result;
    }

    static ff_span text_span(ff_string_view text)
    {
        return make_span(text.data, text.count);
    }

    static void assert_status(ff_resource_status expected, const ff_resource_data& actual)
    {
        Assert::AreEqual((int)expected, (int)actual.status);
        if (expected != ff_resource_status_found)
        {
            Assert::AreEqual((int)ff_resource_data_type_none, (int)actual.type);
            Assert::AreEqual((size_t)0, actual.bytes.size);
            Assert::IsNull(actual.bytes.data);
            Assert::IsNull(actual.dict.data);
        }
    }

    static void assert_bytes(const ff_resource_data& actual, ff_span expected)
    {
        assert_status(ff_resource_status_found, actual);
        Assert::AreEqual((int)ff_resource_data_type_bytes, (int)actual.type);
        Assert::AreEqual(expected.size, actual.bytes.size);
        Assert::IsNull(actual.dict.data);
        if (expected.size)
        {
            Assert::IsNotNull(actual.bytes.data);
            Assert::IsTrue(::memcmp(actual.bytes.data, expected.data, expected.size) == 0);
        }
    }

    static void assert_text(const ff_resource_data& actual, ff_string_view expected)
    {
        assert_bytes(actual, text_span(expected));
    }

    static void assert_inert(const ff_resource_cache& cache)
    {
        Assert::AreEqual((int)ff_resource_cache_type_none, (int)cache.type);
        Assert::IsNull(cache.pack.data);
        Assert::IsNull(cache.mapping.file);
        Assert::IsNull(cache.mapping.mapping);
        Assert::IsNull(cache.mapping.base);
        Assert::AreEqual((size_t)0, cache.mapping.size);
    }

    static void set_bytes(ff_dict* dict, ff_string_view name, ff_span bytes)
    {
        const ff_value value = ff_value_new_data(bytes);
        ff_dict_set(dict, name, &value);
    }

    static ff_span save_pack(ff_dict* source, ff_arena* arena)
    {
        ff_idict dict{};
        ff_idict_init(&dict, arena, source);
        Assert::IsNotNull(dict.data);
        const ff_span saved = ff_idict_save(&dict, arena);
        Assert::IsNotNull(saved.data);
        Assert::IsTrue(saved.size > 0);
        return saved;
    }

    static ff_span make_text_pack(ff_arena* arena, ff_string_view name, ff_string_view text)
    {
        ff_dict source{};
        ff_dict_init(&source, arena);
        set_bytes(&source, name, text_span(text));
        return save_pack(&source, arena);
    }

    static ff_span make_test_pack(ff_arena* arena)
    {
        static const uint8_t binary[] = { 0, 1, 127, 128, 255 };
        ff_dict source{};
        ff_dict_init(&source, arena);
        set_bytes(&source, FF_SVL("raw.bin"), make_span(binary, sizeof(binary)));
        set_bytes(&source, FF_SVL("nested/file.bin"), text_span(FF_SVL("nested")));
        set_bytes(&source, FF_SVL("empty.bin"), ff_span_empty());

        ff_dict object{};
        ff_dict_init(&object, arena);
        const ff_value number = ff_value_new_int32(42);
        const ff_value label = ff_value_new_string(FF_SVL("resource dictionary"));
        ff_dict_set(&object, FF_SVL("number"), &number);
        ff_dict_set(&object, FF_SVL("label"), &label);
        const ff_value object_value = ff_value_new_dict(&object);
        const ff_value empty_object = ff_value_new_idict(ff_idict_empty());
        ff_dict_set(&source, FF_SVL("object"), &object_value);
        ff_dict_set(&source, FF_SVL("empty_object"), &empty_object);
        ff_dict_set(&source, FF_SVL("unsupported"), &number);
        return save_pack(&source, arena);
    }

    struct scoped_directory
    {
        scoped_arena storage;
        ff_string_view root{};
        ff_string_view* files{};
        ff_string_view* directories{};

        scoped_directory()
        {
            static volatile long next_id;
            wchar_t current_directory[MAX_PATH]{};
            const DWORD count = ::GetCurrentDirectoryW((DWORD)std::size(current_directory), current_directory);
            Assert::IsTrue(count > 0 && count < std::size(current_directory));

            wchar_t directory[MAX_PATH]{};
            const int length = ::_snwprintf_s(directory, std::size(directory), _TRUNCATE,
                L"%s\\ff_resources_test_%lu_%llu_%ld", current_directory,
                ::GetCurrentProcessId(), (unsigned long long)::GetTickCount64(),
                ::InterlockedIncrement(&next_id));
            Assert::IsTrue(length > 0);

            this->root = ff_wide_to_utf8(ff_wz_view(directory), &this->storage.value, true);
            this->files = ff_array_init(ff_string_view, &this->storage.value);
            this->directories = ff_array_init(ff_string_view, &this->storage.value);
            Assert::IsTrue(::CreateDirectoryW(directory, nullptr) != FALSE);
        }

        ~scoped_directory()
        {
            for (size_t i = 0; i < ff_array_count(this->files); i++)
            {
                ::DeleteFileW(this->wide(this->files[i]).data);
            }

            for (size_t i = ff_array_count(this->directories); i > 0; i--)
            {
                ::RemoveDirectoryW(this->wide(this->directories[i - 1]).data);
            }

            ::RemoveDirectoryW(this->wide(this->root).data);
        }

        ff_wstring_view wide(ff_string_view path)
        {
            return ff_utf8_to_wide(path, &this->storage.value, true);
        }

        ff_string_view path(ff_string_view name)
        {
            ff_string_builder builder{};
            ff_string_builder_init_string(&builder, &this->storage.value, this->root);
            ff_string_builder_append_char(&builder, '\\');
            ff_string_builder_append(&builder, name);
            return ff_string_builder_copy(&builder);
        }

        ff_string_view add_directory(ff_string_view name)
        {
            const ff_string_view result = this->path(name);
            ff_array_push(this->directories, result);
            Assert::IsTrue(::CreateDirectoryW(this->wide(result).data, nullptr) != FALSE);
            return result;
        }

        ff_string_view write(ff_string_view name, ff_span contents)
        {
            const ff_string_view result = this->path(name);
            ff_array_push(this->files, result);
            scoped_stream stream;
            Assert::IsTrue(ff_stream_init_write_file(&stream.value, result));
            Assert::IsTrue(ff_stream_write(&stream.value, contents));
            return result;
        }

        ff_string_view write_text(ff_string_view name, ff_string_view text)
        {
            return this->write(name, text_span(text));
        }

        void remove_file(ff_string_view name)
        {
            Assert::IsTrue(::DeleteFileW(this->wide(this->path(name)).data) != FALSE);
        }
    };

    TEST_CLASS(resources_tests)
    {
    public:
        TEST_METHOD(memory_pack_copies_input_and_outlives_source_arena)
        {
            scoped_cache cache;
            ff_resource_data retained{};
            {
                scoped_arena source;
                const ff_span saved = make_test_pack(&source.value);
                Assert::IsTrue(ff_resource_cache_init_pack_data(&cache.value, saved));
                retained = ff_resource_cache_get(&cache.value, FF_SVL("nested/file.bin"));
                assert_text(retained, FF_SVL("nested"));
                ::memset((void*)saved.data, 0, saved.size);
                assert_text(ff_resource_cache_get(&cache.value, FF_SVL("nested/file.bin")), FF_SVL("nested"));
            }

            assert_text(retained, FF_SVL("nested"));
            const ff_resource_data again = ff_resource_cache_get(&cache.value, FF_SVL("nested/file.bin"));
            assert_text(again, FF_SVL("nested"));
            Assert::IsTrue(retained.bytes.data == again.bytes.data);
            const ff_resource_data object = ff_resource_cache_get(&cache.value, FF_SVL("object"));
            assert_status(ff_resource_status_found, object);
            Assert::AreEqual((int)ff_resource_data_type_dict, (int)object.type);
            const ff_ivalue* number = ff_idict_get(&object.dict, FF_SVL("number"));
            Assert::IsNotNull(number);
            Assert::AreEqual(42, number->i32);
        }

        TEST_METHOD(pack_entries_return_binary_and_dictionary_data)
        {
            scoped_arena source;
            scoped_cache cache;
            Assert::IsTrue(ff_resource_cache_init_pack_data(&cache.value, make_test_pack(&source.value)));
            const uint8_t binary[] = { 0, 1, 127, 128, 255 };
            assert_bytes(ff_resource_cache_get(&cache.value, FF_SVL("raw.bin")), make_span(binary, sizeof(binary)));

            const ff_resource_data object = ff_resource_cache_get(&cache.value, FF_SVL("object"));
            assert_status(ff_resource_status_found, object);
            Assert::AreEqual((int)ff_resource_data_type_dict, (int)object.type);
            Assert::IsNotNull(object.dict.data);
            Assert::AreEqual((size_t)0, object.bytes.size);
            Assert::IsNull(object.bytes.data);
            const ff_ivalue* number = ff_idict_get(&object.dict, FF_SVL("number"));
            const ff_ivalue* label = ff_idict_get(&object.dict, FF_SVL("label"));
            Assert::IsNotNull(number);
            Assert::IsNotNull(label);
            Assert::AreEqual((int)ff_value_type_int32, (int)number->type);
            Assert::AreEqual(42, number->i32);
            Assert::IsTrue(ff_string_equal(ff_ivalue_as_string(label, &object.dict), FF_SVL("resource dictionary")));
            Assert::IsNull(ff_idict_get(&object.dict, FF_SVL("missing")));
        }

        TEST_METHOD(pack_distinguishes_missing_empty_and_unsupported_entries)
        {
            scoped_arena source;
            scoped_cache cache;
            Assert::IsTrue(ff_resource_cache_init_pack_data(&cache.value, make_test_pack(&source.value)));
            assert_status(ff_resource_status_not_found, ff_resource_cache_get(&cache.value, FF_SVL("missing")));
            assert_bytes(ff_resource_cache_get(&cache.value, FF_SVL("empty.bin")), ff_span_empty());
            const ff_resource_data empty_object = ff_resource_cache_get(&cache.value, FF_SVL("empty_object"));
            assert_status(ff_resource_status_found, empty_object);
            Assert::AreEqual((int)ff_resource_data_type_dict, (int)empty_object.type);
            Assert::IsNotNull(empty_object.dict.data);
            Assert::IsNull(ff_idict_get(&empty_object.dict, FF_SVL("missing")));
            assert_status(ff_resource_status_error, ff_resource_cache_get(&cache.value, FF_SVL("unsupported")));
        }

        TEST_METHOD(empty_pack_is_valid_and_has_no_entries)
        {
            scoped_arena source;
            ff_dict empty{};
            ff_dict_init(&empty, &source.value);
            scoped_cache cache;
            Assert::IsTrue(ff_resource_cache_init_pack_data(&cache.value, save_pack(&empty, &source.value)));
            assert_status(ff_resource_status_not_found, ff_resource_cache_get(&cache.value, FF_SVL("missing")));
        }

        TEST_METHOD(pack_file_reads_or_maps_and_releases_storage)
        {
            scoped_directory directory;
            scoped_arena source;
            const ff_string_view path = directory.write(FF_SVL("resources.pack"), make_test_pack(&source.value));
            scoped_cache cache;
            Assert::IsTrue(ff_resource_cache_init_pack_file(&cache.value, path));
            Assert::AreEqual((int)ff_resource_cache_type_pack, (int)cache.value.type);
            assert_text(ff_resource_cache_get(&cache.value, FF_SVL("nested/file.bin")), FF_SVL("nested"));
#if PROFILE_APP
            Assert::IsNull(cache.value.mapping.base);
            directory.write_text(FF_SVL("resources.pack"), FF_SVL("changed file"));
            assert_text(ff_resource_cache_get(&cache.value, FF_SVL("nested/file.bin")), FF_SVL("nested"));
#else
            Assert::IsNotNull(cache.value.mapping.base);
            Assert::IsTrue(cache.value.mapping.size > 0);
#endif
            ff_resource_cache_destroy(&cache.value);
            assert_inert(cache.value);
            ff_resource_cache_destroy(&cache.value);
            directory.remove_file(FF_SVL("resources.pack"));
        }

        TEST_METHOD(corrupt_pack_initialization_is_inert_and_destroy_is_repeatable)
        {
            scoped_directory directory;
            scoped_arena source;
            const ff_span valid = make_test_pack(&source.value);
            const uint8_t corrupt[16]{};
            const ff_span invalid_inputs[] =
            {
                ff_span_empty(),
                make_span(nullptr, 1),
                make_span(corrupt, sizeof(corrupt)),
                make_span(valid.data, valid.size - 1),
                make_span(valid.data, SIZE_MAX),
                make_span(valid.data, SIZE_MAX - FF_IDICT_MAX_ALIGN),
            };

            scoped_cache cache;
            ff_resource_cache_destroy(&cache.value);
            for (const ff_span input : invalid_inputs)
            {
                Assert::IsFalse(ff_resource_cache_init_pack_data(&cache.value, input));
                assert_inert(cache.value);
                ff_resource_cache_destroy(&cache.value);
                ff_resource_cache_destroy(&cache.value);
            }

            ((uint8_t*)valid.data)[valid.size - 1] ^= 1;
            Assert::IsFalse(ff_resource_cache_init_pack_data(&cache.value, valid));
            assert_inert(cache.value);
            const ff_string_view corrupt_file = directory.write(FF_SVL("corrupt.pack"), valid);
            Assert::IsFalse(ff_resource_cache_init_pack_file(&cache.value, corrupt_file));
            assert_inert(cache.value);
            Assert::IsFalse(ff_resource_cache_init_pack_file(&cache.value, directory.path(FF_SVL("missing.pack"))));
            assert_inert(cache.value);
            Assert::IsFalse(ff_resource_cache_init_directory(&cache.value, directory.path(FF_SVL("missing"))));
            assert_inert(cache.value);
            Assert::IsFalse(ff_resource_cache_init_directory(&cache.value, corrupt_file));
            assert_inert(cache.value);
            Assert::IsFalse(ff_resource_cache_init_directory(&cache.value, FF_SVL("bad\0path")));
            assert_inert(cache.value);
            Assert::IsFalse(ff_resource_cache_init_pack_file(&cache.value, ff_string_view{}));
            assert_inert(cache.value);
            ff_resource_cache_destroy(&cache.value);
            ff_resource_cache_destroy(&cache.value);
            Assert::IsTrue(ff_resource_cache_init_pack_data(&cache.value,
                make_text_pack(&source.value, FF_SVL("recovered"), FF_SVL("valid"))));
            assert_text(ff_resource_cache_get(&cache.value, FF_SVL("recovered")), FF_SVL("valid"));
        }

        TEST_METHOD(directory_reads_lazily_and_caches_successful_results)
        {
            scoped_directory directory;
            directory.write_text(FF_SVL("file.bin"), FF_SVL("initial"));
            scoped_cache cache;
            Assert::IsTrue(ff_resource_cache_init_directory(&cache.value, directory.root));
            directory.write_text(FF_SVL("file.bin"), FF_SVL("before first lookup"));
            const ff_resource_data first = ff_resource_cache_get(&cache.value, FF_SVL("file.bin"));
            assert_text(first, FF_SVL("before first lookup"));
            directory.write_text(FF_SVL("file.bin"), FF_SVL("after first lookup"));
            const ff_resource_data second = ff_resource_cache_get(&cache.value, FF_SVL("file.bin"));
            assert_text(second, FF_SVL("before first lookup"));
            Assert::IsTrue(first.bytes.data == second.bytes.data);
            directory.remove_file(FF_SVL("file.bin"));
            assert_text(ff_resource_cache_get(&cache.value, FF_SVL("file.bin")), FF_SVL("before first lookup"));
            assert_text(first, FF_SVL("before first lookup"));
        }

        TEST_METHOD(directory_distinguishes_empty_files_and_does_not_cache_misses)
        {
            scoped_directory directory;
            const uint8_t binary[] = { 0, 1, 127, 128, 255 };
            directory.write(FF_SVL("empty.bin"), ff_span_empty());
            directory.write(FF_SVL("binary.bin"), make_span(binary, sizeof(binary)));
            scoped_cache cache;
            Assert::IsTrue(ff_resource_cache_init_directory(&cache.value, directory.root));
            assert_bytes(ff_resource_cache_get(&cache.value, FF_SVL("empty.bin")), ff_span_empty());
            assert_bytes(ff_resource_cache_get(&cache.value, FF_SVL("binary.bin")), make_span(binary, sizeof(binary)));
            assert_status(ff_resource_status_not_found, ff_resource_cache_get(&cache.value, FF_SVL("late.bin")));
            directory.write_text(FF_SVL("late.bin"), FF_SVL("created later"));
            assert_text(ff_resource_cache_get(&cache.value, FF_SVL("late.bin")), FF_SVL("created later"));
            directory.remove_file(FF_SVL("empty.bin"));
            assert_bytes(ff_resource_cache_get(&cache.value, FF_SVL("empty.bin")), ff_span_empty());
        }

        TEST_METHOD(directory_cache_growth_preserves_returned_bytes)
        {
            scoped_directory directory;
            directory.write_text(FF_SVL("first"), FF_SVL("retained bytes"));
            scoped_cache cache;
            Assert::IsTrue(ff_resource_cache_init_directory(&cache.value, directory.root));
            const ff_resource_data first = ff_resource_cache_get(&cache.value, FF_SVL("first"));
            const size_t initial_capacity = cache.value.files.capacity;
            for (size_t i = 0; i < 96; i++)
            {
                char name[32]{};
                Assert::IsTrue(::_snprintf_s(name, std::size(name), _TRUNCATE, "file_%zu", i) > 0);
                directory.write_text(ff_sz_view(name), FF_SVL("additional bytes"));
                assert_text(ff_resource_cache_get(&cache.value, ff_sz_view(name)), FF_SVL("additional bytes"));
            }
            Assert::IsTrue(cache.value.files.capacity > initial_capacity);
            directory.remove_file(FF_SVL("first"));
            const ff_resource_data again = ff_resource_cache_get(&cache.value, FF_SVL("first"));
            Assert::IsTrue(first.bytes.data == again.bytes.data);
            assert_text(first, FF_SVL("retained bytes"));
            assert_text(again, FF_SVL("retained bytes"));
        }

        TEST_METHOD(separators_normalize_for_directory_and_pack_lookups)
        {
            scoped_directory directory;
            directory.add_directory(FF_SVL("nested"));
            directory.write_text(FF_SVL("nested\\file.bin"), FF_SVL("nested"));
            scoped_cache folder;
            Assert::IsTrue(ff_resource_cache_init_directory(&folder.value, directory.root));
            const ff_resource_data slash = ff_resource_cache_get(&folder.value, FF_SVL("nested/file.bin"));
            const ff_resource_data backslash = ff_resource_cache_get(&folder.value, FF_SVL("nested\\file.bin"));
            assert_text(slash, FF_SVL("nested"));
            assert_text(backslash, FF_SVL("nested"));
            Assert::IsTrue(slash.bytes.data == backslash.bytes.data);

            scoped_arena source;
            scoped_cache pack;
            Assert::IsTrue(ff_resource_cache_init_pack_data(&pack.value, make_test_pack(&source.value)));
            const ff_resource_data pack_slash = ff_resource_cache_get(&pack.value, FF_SVL("nested/file.bin"));
            const ff_resource_data pack_backslash = ff_resource_cache_get(&pack.value, FF_SVL("nested\\file.bin"));
            assert_text(pack_slash, FF_SVL("nested"));
            assert_text(pack_backslash, FF_SVL("nested"));
            Assert::IsTrue(pack_slash.bytes.data == pack_backslash.bytes.data);
        }

        TEST_METHOD(invalid_relative_paths_return_errors_without_asserting)
        {
            scoped_directory directory;
            scoped_arena source;
            scoped_cache folder;
            scoped_cache pack;
            scoped_module module;
            scoped_registry registry;
            Assert::IsTrue(ff_resource_cache_init_directory(&folder.value, directory.root));
            const ff_span saved = make_test_pack(&source.value);
            Assert::IsTrue(ff_resource_cache_init_pack_data(&pack.value, saved));
            Assert::IsTrue(ff_resource_module_init(&module.value, FF_SVL("valid")));
            Assert::IsTrue(ff_resource_module_add_pack_data(&module.value, saved));
            Assert::IsTrue(ff_resources_add_module(&registry.value, &module.value));
            const ff_string_view invalid_paths[] =
            {
                FF_SVL(""), FF_SVL("."), FF_SVL(".."), FF_SVL("../raw.bin"), FF_SVL("nested/../raw.bin"),
                FF_SVL("nested\\..\\raw.bin"), FF_SVL("./raw.bin"), FF_SVL("nested/./file.bin"),
                FF_SVL("/raw.bin"), FF_SVL("\\raw.bin"), FF_SVL("C:\\raw.bin"), FF_SVL("C:raw.bin"),
                FF_SVL("\\\\server\\share\\raw.bin"), FF_SVL("raw.bin:stream"), FF_SVL("nested//file.bin"),
                FF_SVL("nested\\\\file.bin"), FF_SVL("nested/"), FF_SVL("raw.bin."), FF_SVL("raw.bin "),
                FF_SVL("bad*name"), FF_SVL("bad?name"), FF_SVL("bad\"name"), FF_SVL("bad<name"),
                FF_SVL("bad>name"), FF_SVL("bad|name"), FF_SVL("bad\0name"), FF_SVL("bad\nname"),
            };
            for (const ff_string_view name : invalid_paths)
            {
                assert_status(ff_resource_status_error, ff_resource_cache_get(&folder.value, name));
                assert_status(ff_resource_status_error, ff_resource_cache_get(&pack.value, name));
                assert_status(ff_resource_status_error, ff_resource_module_get(&module.value, name));
                ff_string_builder qualified{};
                ff_string_builder_init_string(&qualified, &source.value, FF_SVL("valid:"));
                ff_string_builder_append(&qualified, name);
                assert_status(ff_resource_status_error,
                    ff_resources_get(&registry.value, ff_string_builder_view(&qualified)));
            }

            ff_string_view null_name{};
            null_name.count = 1;
            assert_status(ff_resource_status_error, ff_resource_cache_get(&folder.value, null_name));
            assert_status(ff_resource_status_error, ff_resource_cache_get(&pack.value, null_name));
            assert_status(ff_resource_status_error, ff_resource_module_get(&module.value, null_name));
            assert_status(ff_resource_status_error, ff_resources_get(&registry.value, null_name));
        }

        TEST_METHOD(directory_precedence_and_pack_fallback_follow_registration_order)
        {
            scoped_directory first;
            scoped_directory second;
            first.write_text(FF_SVL("shared"), FF_SVL("first folder"));
            first.write(FF_SVL("empty"), ff_span_empty());
            second.write_text(FF_SVL("shared"), FF_SVL("second folder"));
            second.write_text(FF_SVL("second_only"), FF_SVL("second folder"));

            scoped_arena source;
            ff_dict contents{};
            ff_dict_init(&contents, &source.value);
            set_bytes(&contents, FF_SVL("shared"), text_span(FF_SVL("pack")));
            set_bytes(&contents, FF_SVL("second_only"), text_span(FF_SVL("pack")));
            set_bytes(&contents, FF_SVL("empty"), text_span(FF_SVL("not empty")));
            set_bytes(&contents, FF_SVL("pack_only"), text_span(FF_SVL("pack fallback")));
            const ff_span saved = save_pack(&contents, &source.value);
            const ff_string_view pack_path = second.write(FF_SVL("resources.pack"), saved);
            scoped_module module;
            Assert::IsTrue(ff_resource_module_init(&module.value, FF_SVL("ordered")));
            Assert::IsTrue(ff_resource_module_add_directory(&module.value, first.root));
            Assert::IsTrue(ff_resource_module_add_directory(&module.value, second.root));
            Assert::IsTrue(ff_resource_module_add_pack_file(&module.value, pack_path));
            assert_text(ff_resource_module_get(&module.value, FF_SVL("shared")), FF_SVL("first folder"));
            assert_text(ff_resource_module_get(&module.value, FF_SVL("second_only")), FF_SVL("second folder"));
            assert_text(ff_resource_module_get(&module.value, FF_SVL("pack_only")), FF_SVL("pack fallback"));
            assert_bytes(ff_resource_module_get(&module.value, FF_SVL("empty")), ff_span_empty());
            assert_status(ff_resource_status_not_found, ff_resource_module_get(&module.value, FF_SVL("missing")));
        }

        TEST_METHOD(source_errors_stop_fallback_and_are_not_cached)
        {
            scoped_directory directory;
            const ff_string_view blocked = directory.add_directory(FF_SVL("blocked"));
            scoped_arena source;
            scoped_module module;
            Assert::IsTrue(ff_resource_module_init(&module.value, FF_SVL("errors")));
            Assert::IsTrue(ff_resource_module_add_directory(&module.value, directory.root));
            Assert::IsTrue(ff_resource_module_add_pack_data(&module.value,
                make_text_pack(&source.value, FF_SVL("blocked"), FF_SVL("fallback"))));
            assert_status(ff_resource_status_error, ff_resource_module_get(&module.value, FF_SVL("blocked")));
            Assert::IsTrue(::RemoveDirectoryW(directory.wide(blocked).data) != FALSE);
            assert_text(ff_resource_module_get(&module.value, FF_SVL("blocked")), FF_SVL("fallback"));
        }

        TEST_METHOD(unsupported_pack_entries_stop_fallback)
        {
            scoped_arena source;
            scoped_module module;
            Assert::IsTrue(ff_resource_module_init(&module.value, FF_SVL("errors")));
            Assert::IsTrue(ff_resource_module_add_pack_data(&module.value, make_test_pack(&source.value)));
            Assert::IsTrue(ff_resource_module_add_pack_data(&module.value,
                make_text_pack(&source.value, FF_SVL("unsupported"), FF_SVL("fallback"))));
            assert_status(ff_resource_status_error, ff_resource_module_get(&module.value, FF_SVL("unsupported")));
        }

        TEST_METHOD(module_namespaces_duplicate_registration_and_borrowed_lifetimes)
        {
            scoped_arena source;
            scoped_module alpha;
            scoped_module beta;
            scoped_module duplicate;
            Assert::IsTrue(ff_resource_module_init(&alpha.value, FF_SVL("alpha")));
            Assert::IsTrue(ff_resource_module_init(&beta.value, FF_SVL("beta")));
            Assert::IsTrue(ff_resource_module_init(&duplicate.value, FF_SVL("alpha")));
            Assert::IsTrue(ff_resource_module_add_pack_data(&alpha.value,
                make_text_pack(&source.value, FF_SVL("same"), FF_SVL("alpha data"))));
            Assert::IsTrue(ff_resource_module_add_pack_data(&beta.value,
                make_text_pack(&source.value, FF_SVL("same"), FF_SVL("beta data"))));
            {
                scoped_registry registry;
                Assert::IsTrue(ff_resources_add_module(&registry.value, &alpha.value));
                Assert::IsTrue(ff_resources_add_module(&registry.value, &beta.value));
                Assert::IsFalse(ff_resources_add_module(&registry.value, &alpha.value));
                Assert::IsFalse(ff_resources_add_module(&registry.value, &duplicate.value));
                Assert::AreEqual((size_t)2, ff_array_count(registry.value.modules));
                Assert::IsTrue(ff_resources_find_module(&registry.value, FF_SVL("alpha")) == &alpha.value);
                Assert::IsTrue(ff_resources_find_module(&registry.value, FF_SVL("beta")) == &beta.value);
                Assert::IsNull(ff_resources_find_module(&registry.value, FF_SVL("unknown")));
                assert_text(ff_resources_get(&registry.value, FF_SVL("alpha:same")), FF_SVL("alpha data"));
                assert_text(ff_resources_get(&registry.value, FF_SVL("beta:same")), FF_SVL("beta data"));
                assert_status(ff_resource_status_not_found, ff_resources_get(&registry.value, FF_SVL("unknown:same")));
                assert_status(ff_resource_status_not_found, ff_resources_get(&registry.value, FF_SVL("alpha:missing")));
                const ff_string_view invalid_names[] =
                {
                    FF_SVL(""), FF_SVL("same"), FF_SVL(":same"), FF_SVL("alpha:"),
                    FF_SVL("bad-name:same"), FF_SVL("alpha:same:stream"),
                };
                for (const ff_string_view name : invalid_names)
                {
                    assert_status(ff_resource_status_error, ff_resources_get(&registry.value, name));
                }
                assert_status(ff_resource_status_error, ff_resource_module_get(&alpha.value, FF_SVL("alpha:same")));
                ff_resources_destroy(&registry.value);
                ff_resources_destroy(&registry.value);
                Assert::IsFalse(registry.value.initialized);
            }

            Assert::IsTrue(alpha.value.initialized);
            Assert::IsTrue(beta.value.initialized);
            assert_text(ff_resource_module_get(&alpha.value, FF_SVL("same")), FF_SVL("alpha data"));
            assert_text(ff_resource_module_get(&beta.value, FF_SVL("same")), FF_SVL("beta data"));
        }

        TEST_METHOD(sealing_prevents_registry_and_module_cache_registration)
        {
            scoped_directory directory;
            scoped_arena source;
            const ff_span saved = make_test_pack(&source.value);
            const ff_string_view pack_path = directory.write(FF_SVL("resources.pack"), saved);
            scoped_module module;
            scoped_module late;
            scoped_registry registry;
            Assert::IsTrue(ff_resource_module_init(&module.value, FF_SVL("registered")));
            Assert::IsTrue(ff_resource_module_init(&late.value, FF_SVL("late")));
            Assert::IsTrue(ff_resource_module_add_pack_data(&module.value, saved));
            Assert::IsTrue(ff_resources_add_module(&registry.value, &module.value));
            ff_resources_seal(&registry.value);
            ff_resources_seal(&registry.value);
            Assert::IsTrue(registry.value.sealed);
            Assert::IsTrue(module.value.sealed);
            Assert::IsFalse(late.value.sealed);
            Assert::IsFalse(ff_resources_add_module(&registry.value, &late.value));
            Assert::IsFalse(ff_resource_module_add_directory(&module.value, directory.root));
            Assert::IsFalse(ff_resource_module_add_pack_file(&module.value, pack_path));
            Assert::IsFalse(ff_resource_module_add_pack_data(&module.value, saved));
            Assert::AreEqual((size_t)1, ff_array_count(registry.value.modules));
            Assert::AreEqual((size_t)1, ff_array_count(module.value.caches));
            assert_text(ff_resources_get(&registry.value, FF_SVL("registered:nested/file.bin")), FF_SVL("nested"));
            Assert::IsTrue(ff_resource_module_add_pack_data(&late.value, saved));
        }

        TEST_METHOD(invalid_module_names_and_failed_cache_additions_leave_safe_state)
        {
            scoped_directory directory;
            scoped_module module;
            const ff_string_view invalid_names[] =
            {
                FF_SVL(""), FF_SVL("bad-name"), FF_SVL("bad/name"), FF_SVL("bad\\name"),
                FF_SVL("bad:name"), FF_SVL("bad name"), FF_SVL("bad.name"), FF_SVL("bad\0name"),
            };
            for (const ff_string_view name : invalid_names)
            {
                Assert::IsFalse(ff_resource_module_init(&module.value, name));
                Assert::IsFalse(module.value.initialized);
                ff_resource_module_destroy(&module.value);
                ff_resource_module_destroy(&module.value);
            }
            Assert::IsTrue(ff_resource_module_init(&module.value, FF_SVL("Module_123")));
            Assert::IsFalse(ff_resource_module_add_directory(&module.value, directory.path(FF_SVL("missing"))));
            Assert::IsFalse(ff_resource_module_add_pack_file(&module.value, directory.path(FF_SVL("missing.pack"))));
            Assert::IsFalse(ff_resource_module_add_pack_data(&module.value, ff_span_empty()));
            Assert::AreEqual((size_t)0, ff_array_count(module.value.caches));
            assert_status(ff_resource_status_not_found, ff_resource_module_get(&module.value, FF_SVL("missing")));
            Assert::IsTrue(ff_resource_module_add_directory(&module.value, directory.root));
            directory.write_text(FF_SVL("recovered"), FF_SVL("usable"));
            assert_text(ff_resource_module_get(&module.value, FF_SVL("recovered")), FF_SVL("usable"));
            ff_resource_module_destroy(&module.value);
            ff_resource_module_destroy(&module.value);
            Assert::IsFalse(module.value.initialized);
            Assert::IsNull(module.value.caches);
        }

        TEST_METHOD(cache_growth_preserves_cache_and_result_pointers)
        {
            scoped_arena source;
            const ff_span saved = make_test_pack(&source.value);
            scoped_module module;
            Assert::IsTrue(ff_resource_module_init(&module.value, FF_SVL("growth")));
            Assert::IsTrue(ff_resource_module_add_pack_data(&module.value, saved));
            ff_resource_cache* first_cache = module.value.caches[0];
            const size_t initial_capacity = ff_array_capacity(module.value.caches);
            const ff_resource_data first = ff_resource_module_get(&module.value, FF_SVL("nested/file.bin"));
            const ff_resource_data dictionary = ff_resource_module_get(&module.value, FF_SVL("object"));
            for (size_t i = 0; i < 96; i++)
            {
                Assert::IsTrue(ff_resource_module_add_pack_data(&module.value, saved));
            }
            Assert::IsTrue(ff_array_capacity(module.value.caches) > initial_capacity);
            Assert::IsTrue(module.value.caches[0] == first_cache);
            assert_text(first, FF_SVL("nested"));
            assert_text(ff_resource_cache_get(first_cache, FF_SVL("nested/file.bin")), FF_SVL("nested"));
            const ff_resource_data again = ff_resource_module_get(&module.value, FF_SVL("nested/file.bin"));
            Assert::IsTrue(first.bytes.data == again.bytes.data);
            const ff_resource_data dictionary_again = ff_resource_module_get(&module.value, FF_SVL("object"));
            Assert::IsTrue(dictionary.dict.data == dictionary_again.dict.data);
            const ff_ivalue* number = ff_idict_get(&dictionary.dict, FF_SVL("number"));
            Assert::IsNotNull(number);
            Assert::AreEqual(42, number->i32);
        }

        TEST_METHOD(registry_growth_preserves_borrowed_module_pointers_and_copied_names)
        {
            scoped_module modules[96];
            scoped_registry registry;
            const size_t initial_capacity = ff_array_capacity(registry.value.modules);
            for (size_t i = 0; i < std::size(modules); i++)
            {
                char name[32]{};
                Assert::IsTrue(::_snprintf_s(name, std::size(name), _TRUNCATE, "module_%zu", i) > 0);
                Assert::IsTrue(ff_resource_module_init(&modules[i].value, ff_sz_view(name)));
                Assert::IsTrue(ff_resources_add_module(&registry.value, &modules[i].value));
                ::memset(name, 'x', sizeof(name));
            }
            Assert::IsTrue(ff_array_capacity(registry.value.modules) > initial_capacity);
            Assert::AreEqual(std::size(modules), ff_array_count(registry.value.modules));
            for (size_t i = 0; i < std::size(modules); i++)
            {
                char name[32]{};
                Assert::IsTrue(::_snprintf_s(name, std::size(name), _TRUNCATE, "module_%zu", i) > 0);
                Assert::IsTrue(registry.value.modules[i] == &modules[i].value);
                Assert::IsTrue(ff_resources_find_module(&registry.value, ff_sz_view(name)) == &modules[i].value);
            }
        }

        TEST_METHOD(global_startup_base_registration_sealing_and_teardown_repeat)
        {
            for (size_t lifetime = 0; lifetime < 3; lifetime++)
            {
                scoped_module module;
                Assert::IsTrue(ff_resource_module_init(&module.value, FF_SVL("extra")));
                ff_resources* resources = nullptr;
                {
                    scoped_global_resources global;
                    global.initialized = ff_resources_global_init();
                    Assert::IsTrue(global.initialized);
                    resources = ff_resources_global();
                    Assert::IsNotNull(resources);
                    Assert::IsTrue(resources->initialized);
                    Assert::IsFalse(resources->sealed);
                    ff_resource_module* base = ff_resources_find_module(resources, FF_SVL("base"));
                    Assert::IsNotNull(base);
                    Assert::IsTrue(base->initialized);
                    Assert::IsFalse(base->sealed);
                    Assert::IsTrue(ff_string_equal(base->name, FF_SVL("base")));
#if PROFILE_APP
                    Assert::AreEqual((size_t)1, ff_array_count(base->caches));
                    Assert::AreEqual((int)ff_resource_cache_type_directory, (int)base->caches[0]->type);
#else
                    Assert::AreEqual((size_t)0, ff_array_count(base->caches));
#endif
                    assert_status(ff_resource_status_not_found,
                        ff_resources_get(resources, FF_SVL("base:ff_resources_test_missing_resource")));
                    Assert::IsTrue(ff_resources_add_module(resources, &module.value));
                    ff_resources_seal(resources);
                    Assert::IsTrue(resources->sealed);
                    Assert::IsTrue(base->sealed);
                    Assert::IsTrue(module.value.sealed);
                    Assert::IsFalse(ff_resources_add_module(resources, &module.value));
                    Assert::IsFalse(ff_resource_module_add_pack_data(base, ff_span_empty()));
                }
                Assert::IsFalse(resources->initialized);
                Assert::IsFalse(resources->sealed);
                Assert::IsNull(resources->modules);
                ff_resources_global_destroy();
                Assert::IsTrue(module.value.initialized);
            }
        }
    };
}
