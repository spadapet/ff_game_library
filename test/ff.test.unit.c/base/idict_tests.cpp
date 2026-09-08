#include "pch.h"

static ff_string_view sv(const char* text)
{
    ff_string_view result{ text, strlen(text) };
    return result;
}

// Every block begins with its own entry count and byte size. Neither is part of the public API, so
// tests read them out of the block the same way the implementation does.
static constexpr size_t block_header_size = 8;
static constexpr size_t entry_size = sizeof(uint64_t) + sizeof(ff_ivalue);

static size_t count_of(const ff_idict* dict)
{
    return (dict && dict->data) ? *(const uint32_t*)dict->data : 0;
}

static size_t size_of(const ff_idict* dict)
{
    return (dict && dict->data) ? ((const uint32_t*)dict->data)[1] : 0;
}

static size_t round_up_to(size_t value, size_t align)
{
    return (value + align - 1) & ~(align - 1);
}

// The data section begins at the format's max alignment, measured from the start of the block.
static size_t data_start_of(size_t count)
{
    return round_up_to(block_header_size + count * entry_size, ff_idict_max_align);
}

// The immutable dict derives everything from one block, so tests derive it the same way.
static const uint64_t* keys_of(const ff_idict& dict)
{
    return (const uint64_t*)((const uint8_t*)dict.data + block_header_size);
}

static const ff_ivalue* values_of(const ff_idict& dict)
{
    return (const ff_ivalue*)(keys_of(dict) + count_of(&dict));
}

static const uint8_t* data_of(const ff_idict& dict)
{
    return (const uint8_t*)dict.data + data_start_of(count_of(&dict));
}

// A block whose values needed no data stops right after them, so its data section is empty.
static size_t data_size_of(const ff_idict& dict)
{
    size_t start = data_start_of(count_of(&dict));
    size_t size = size_of(&dict);
    return (size > start) ? size - start : 0;
}

static bool is_aligned(const void* ptr, size_t align)
{
    return ((uintptr_t)ptr & (align - 1)) == 0;
}

namespace ff::test::base
{
    TEST_CLASS(idict_tests)
    {
    public:
        // ====================================================================
        // Empty
        // ====================================================================
        TEST_METHOD(init_from_empty_dict)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            Assert::AreEqual((size_t)0, count_of(&dict));
            Assert::AreEqual(block_header_size, size_of(&dict));
            Assert::IsNull(ff_idict_get(&dict, sv("anything")));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(init_from_null_source)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, nullptr);

            Assert::AreEqual((size_t)0, count_of(&dict));
            Assert::IsNull(ff_idict_get(&dict, sv("anything")));

            ff_arena_destroy(&arena);
        }

        // ====================================================================
        // Scalars
        // ====================================================================
        TEST_METHOD(scalar_values_round_trip)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            GUID guid;
            memset(&guid, 0xAB, sizeof(guid));

            ff_value null_value = ff_value_new_null();
            ff_value bool_value = ff_value_new_boolean(true);
            ff_value int32_value = ff_value_new_int32(-5);
            ff_value int64_value = ff_value_new_int64(1234567890123LL);
            ff_value float64_value = ff_value_new_float64(2.5);
            ff_value point_value = ff_value_new_point_int32(3, 4);
            ff_value rect_value = ff_value_new_rect_float32(1.0f, 2.0f, 3.0f, 4.0f);
            ff_value guid_value = ff_value_new_guid(guid);

            ff_dict_set(&source, sv("null"), &null_value);
            ff_dict_set(&source, sv("bool"), &bool_value);
            ff_dict_set(&source, sv("int32"), &int32_value);
            ff_dict_set(&source, sv("int64"), &int64_value);
            ff_dict_set(&source, sv("float64"), &float64_value);
            ff_dict_set(&source, sv("point"), &point_value);
            ff_dict_set(&source, sv("rect"), &rect_value);
            ff_dict_set(&source, sv("guid"), &guid_value);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            Assert::AreEqual((size_t)8, count_of(&dict));
            Assert::IsTrue(ff_value_type_null == ff_idict_get(&dict, sv("null"))->type);
            Assert::IsTrue(ff_idict_get(&dict, sv("bool"))->b);
            Assert::AreEqual(-5, ff_idict_get(&dict, sv("int32"))->i32);
            Assert::AreEqual((int64_t)1234567890123LL, ff_idict_get(&dict, sv("int64"))->i64);
            Assert::AreEqual(2.5, ff_idict_get(&dict, sv("float64"))->f64);
            Assert::AreEqual(3, ff_idict_get(&dict, sv("point"))->point_i32[0]);
            Assert::AreEqual(4, ff_idict_get(&dict, sv("point"))->point_i32[1]);
            Assert::AreEqual(4.0f, ff_idict_get(&dict, sv("rect"))->rect_f32[3]);
            Assert::IsTrue(memcmp(&ff_idict_get(&dict, sv("guid"))->guid, &guid, sizeof(guid)) == 0);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(missing_key_returns_null)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value value = ff_value_new_int32(1);
            ff_dict_set(&source, sv("key"), &value);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            Assert::IsNull(ff_idict_get(&dict, sv("missing")));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(duplicate_keys_keep_order)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            for (int i = 0; i < 5; i++)
            {
                ff_value value = ff_value_new_int32(i);
                ff_dict_add(&source, sv("dup"), &value);
            }

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            int expected = 0;
            int visited = 0;

            for (const ff_ivalue* value = ff_idict_get(&dict, sv("dup")); value && visited < 32; value = ff_idict_get_next(&dict, sv("dup"), value))
            {
                Assert::AreEqual(expected++, value->i32);
                visited++;
            }

            Assert::AreEqual(5, visited);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(get_next_on_empty_dict_returns_null)
        {
            ff_idict dict{};

            Assert::IsNull(ff_idict_get_next(&dict, sv("anything"), nullptr));
        }

        TEST_METHOD(get_next_past_the_last_match_returns_null)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            for (int i = 0; i < 3; i++)
            {
                ff_value value = ff_value_new_int32(i);
                ff_dict_add(&source, sv("dup"), &value);
            }

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            const ff_ivalue* last = values_of(dict) + 2;

            Assert::AreEqual(2, last->i32);
            Assert::IsNull(ff_idict_get_next(&dict, sv("dup"), last));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(get_next_only_searches_after_the_given_value)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value a = ff_value_new_int32(1);
            ff_value b = ff_value_new_int32(2);
            ff_dict_set(&source, sv("a"), &a);
            ff_dict_set(&source, sv("b"), &b);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            // "a" lives before "b", so resuming at "b" can never find it again.
            Assert::IsNull(ff_idict_get_next(&dict, sv("a"), values_of(dict) + 1));
            Assert::IsNotNull(ff_idict_get_next(&dict, sv("b"), values_of(dict) + 0));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(get_next_works_inside_a_nested_dict)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict inner{};
            ff_dict_init(&inner, &arena);

            for (int i = 0; i < 4; i++)
            {
                ff_value value = ff_value_new_int32(i * 10);
                ff_dict_add(&inner, sv("dup"), &value);
            }

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value nested = ff_value_new_dict(&inner);
            ff_dict_set(&source, sv("child"), &nested);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            ff_idict child_dict = ff_ivalue_as_dict(ff_idict_get(&dict, sv("child")), &dict);

            int expected = 0;
            int visited = 0;

            for (const ff_ivalue* value = ff_idict_get(&child_dict, sv("dup")); value && visited < 32; value = ff_idict_get_next(&child_dict, sv("dup"), value))
            {
                Assert::AreEqual(expected, value->i32);
                expected += 10;
                visited++;
            }

            Assert::AreEqual(4, visited);

            ff_arena_destroy(&arena);
        }

        // ====================================================================
        // Strings
        // ====================================================================
        TEST_METHOD(string_value_round_trips)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value value = ff_value_new_string(sv("hello world"));
            ff_dict_set(&source, sv("greeting"), &value);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            ff_string_view text = ff_ivalue_as_string(ff_idict_get(&dict, sv("greeting")), &dict);

            Assert::AreEqual((size_t)11, text.count);
            Assert::IsTrue(memcmp(text.data, "hello world", 11) == 0);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(string_bytes_live_inside_the_block)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            char buffer[] = "temporary";
            ff_value value = ff_value_new_string(sv(buffer));
            ff_dict_set(&source, sv("key"), &value);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            ff_string_view text = ff_ivalue_as_string(ff_idict_get(&dict, sv("key")), &dict);

            // The block owns a copy, so it does not alias the caller's buffer.
            Assert::IsTrue(text.data != buffer);
            Assert::IsTrue(text.data >= (const char*)dict.data);
            Assert::IsTrue(text.data + text.count <= (const char*)dict.data + size_of(&dict));

            memset(buffer, 0, sizeof(buffer));
            Assert::IsTrue(memcmp(text.data, "temporary", 9) == 0);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(empty_string_value)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value value = ff_value_new_string(sv(""));
            ff_dict_set(&source, sv("key"), &value);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            ff_string_view text = ff_ivalue_as_string(ff_idict_get(&dict, sv("key")), &dict);

            Assert::AreEqual((size_t)0, text.count);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(strings_are_packed_without_padding)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value first = ff_value_new_string(sv("abc"));
            ff_value second = ff_value_new_string(sv("de"));
            ff_dict_set(&source, sv("first"), &first);
            ff_dict_set(&source, sv("second"), &second);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            // Bytes need no alignment, so the second string starts right after the first.
            Assert::AreEqual((size_t)0, (size_t)values_of(dict)[0].data.offset);
            Assert::AreEqual((size_t)3, (size_t)values_of(dict)[1].data.offset);
            Assert::AreEqual(data_start_of(count_of(&dict)) + 5, size_of(&dict));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(nested_dict_entry_count_is_not_capped_by_the_slice_fields)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict inner{};
            ff_dict_init(&inner, &arena);

            // Well past what the 16 bit item_size field used to hold, back when a nested dict's
            // entry count was recorded in the parent's value instead of in its own block header.
            const int count = 70000;
            char key[32];

            for (int i = 0; i < count; i++)
            {
                sprintf_s(key, "key%d", i);
                ff_value value = ff_value_new_int32(i);
                ff_dict_set(&inner, sv(key), &value);
            }

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value nested = ff_value_new_dict(&inner);
            ff_dict_set(&source, sv("child"), &nested);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            const ff_ivalue* child = ff_idict_get(&dict, sv("child"));
            ff_idict child_dict = ff_ivalue_as_dict(child, &dict);

            // A nested dict is described by its offset alone; everything else comes from its block.
            Assert::AreEqual((size_t)0, (size_t)child->data.count);
            Assert::AreEqual((size_t)0, (size_t)child->data.item_size);
            Assert::AreEqual((size_t)count, count_of(&child_dict));
            Assert::AreEqual(block_header_size + (size_t)count * 32, size_of(&child_dict));

            for (int i = 0; i < count; i++)
            {
                sprintf_s(key, "key%d", i);
                Assert::AreEqual(i, ff_idict_get(&child_dict, sv(key))->i32);
            }

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(string_bytes_are_copied_verbatim)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            // Counted, not terminated: embedded nulls and high bytes have to survive intact.
            const char raw[] = "a\0b\xC3\xA9\xFF";
            ff_string_view raw_view{ raw, sizeof(raw) - 1 };

            ff_value value = ff_value_new_string(raw_view);
            ff_dict_set(&source, sv("key"), &value);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            ff_string_view text = ff_ivalue_as_string(ff_idict_get(&dict, sv("key")), &dict);

            Assert::AreEqual(sizeof(raw) - 1, text.count);
            Assert::IsTrue(memcmp(text.data, raw, text.count) == 0);
            Assert::AreEqual(data_start_of(count_of(&dict)) + sizeof(raw) - 1, size_of(&dict));

            ff_arena_destroy(&arena);
        }

        // ====================================================================
        // Data
        // ====================================================================
        TEST_METHOD(data_value_round_trips)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            uint8_t bytes[5] = { 1, 2, 3, 4, 5 };
            ff_span span{ bytes, sizeof(bytes) };
            ff_value value = ff_value_new_data(span);
            ff_dict_set(&source, sv("blob"), &value);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            const ff_ivalue* found = ff_idict_get(&dict, sv("blob"));

            Assert::IsTrue(ff_value_type_data == found->type);
            Assert::AreEqual((size_t)5, (size_t)found->data.count);
            Assert::AreEqual((size_t)1, (size_t)found->data.item_size);
            Assert::IsTrue(memcmp(data_of(dict) + found->data.offset, bytes, sizeof(bytes)) == 0);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(data_keeps_its_item_alignment)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            // A one byte string first, so an unaligned data item would be caught.
            ff_value text = ff_value_new_string(sv("x"));
            ff_dict_set(&source, sv("text"), &text);

            double numbers[3] = { 1.5, 2.5, 3.5 };
            ff_array_span as{};
            as.data = numbers;
            as.count = 3;
            as.item_size = sizeof(double);
            as.item_align = alignof(double);

            ff_value value = ff_value_new_data_array(as);
            ff_dict_set(&source, sv("numbers"), &value);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            const ff_ivalue* found = ff_idict_get(&dict, sv("numbers"));
            const double* stored = (const double*)(data_of(dict) + found->data.offset);

            Assert::AreEqual((size_t)alignof(double), (size_t)found->data.item_align);
            Assert::IsTrue(is_aligned(stored, alignof(double)));
            Assert::AreEqual(1.5, stored[0]);
            Assert::AreEqual(3.5, stored[2]);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(over_aligned_data_forces_block_alignment)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            alignas(64) uint8_t wide[64] = { 7 };
            ff_array_span as{};
            as.data = wide;
            as.count = 1;
            as.item_size = 64;
            as.item_align = 64;

            ff_value value = ff_value_new_data_array(as);
            ff_dict_set(&source, sv("wide"), &value);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            const ff_ivalue* found = ff_idict_get(&dict, sv("wide"));

            // The whole block has to be allocated strictly enough for its strictest item.
            Assert::IsTrue(is_aligned(dict.data, 64));
            Assert::IsTrue(is_aligned(data_of(dict) + found->data.offset, 64));
            Assert::AreEqual((uint8_t)7, *(data_of(dict) + found->data.offset));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(empty_data_value)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_array_span as{};
            as.data = nullptr;
            as.count = 0;
            as.item_size = sizeof(double);
            as.item_align = alignof(double);

            ff_value value = ff_value_new_data_array(as);
            ff_dict_set(&source, sv("empty"), &value);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            ff_array_span found = ff_ivalue_as_data(ff_idict_get(&dict, sv("empty")), &dict);

            Assert::AreEqual((size_t)0, (size_t)found.count);
            Assert::AreEqual((size_t)sizeof(double), (size_t)found.item_size);
            Assert::AreEqual(block_header_size + count_of(&dict) * 32, size_of(&dict));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(data_with_zero_align_asks_for_nothing)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value text = ff_value_new_string(sv("x"));
            ff_dict_set(&source, sv("text"), &text);

            uint8_t bytes[2] = { 1, 2 };
            ff_array_span as{};
            as.data = bytes;
            as.count = 2;
            as.item_size = 1;
            as.item_align = 0;

            ff_value value = ff_value_new_data_array(as);
            ff_dict_set(&source, sv("blob"), &value);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            ff_array_span found = ff_ivalue_as_data(ff_idict_get(&dict, sv("blob")), &dict);

            // Zero means "no requirement", so the bytes are packed rather than padded out to a word.
            Assert::AreEqual((size_t)1, (size_t)found.item_align);
            Assert::IsTrue(memcmp(found.data, bytes, sizeof(bytes)) == 0);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(alignment_padding_between_items_is_zeroed)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            // Two single byte payloads that each demand 8 byte alignment. Entries are stored in key
            // order, so which one lands first is not fixed, but either way the second is pushed to
            // offset 8 and seven bytes have to be skipped to get there.
            uint8_t first_byte = 0xAB;
            uint8_t second_byte = 0xCD;

            ff_array_span as{};
            as.count = 1;
            as.item_size = 1;
            as.item_align = 8;

            as.data = &first_byte;
            ff_value first = ff_value_new_data_array(as);
            ff_dict_set(&source, sv("first"), &first);

            as.data = &second_byte;
            ff_value second = ff_value_new_data_array(as);
            ff_dict_set(&source, sv("second"), &second);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            const ff_ivalue* first_value = ff_idict_get(&dict, sv("first"));
            const ff_ivalue* second_value = ff_idict_get(&dict, sv("second"));

            Assert::AreEqual((size_t)0, (size_t)first_value->data.offset % 8);
            Assert::AreEqual((size_t)0, (size_t)second_value->data.offset % 8);
            Assert::AreEqual((size_t)8, (size_t)(first_value->data.offset + second_value->data.offset));
            Assert::AreEqual((size_t)9, data_size_of(dict));

            Assert::AreEqual(first_byte, *(const uint8_t*)ff_ivalue_as_data(first_value, &dict).data);
            Assert::AreEqual(second_byte, *(const uint8_t*)ff_ivalue_as_data(second_value, &dict).data);

            // The seven bytes skipped to reach the aligned offset must be zero, not whatever the
            // arena happened to be holding.
            for (size_t i = 1; i < 8; i++)
            {
                Assert::AreEqual((uint8_t)0, data_of(dict)[i]);
            }

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(an_aligned_value_round_trips_whatever_precedes_it)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value text = ff_value_new_string(sv("x"));
            ff_dict_set(&source, sv("text"), &text);

            uint64_t number = 0x1122334455667788ULL;
            ff_array_span as{};
            as.data = &number;
            as.count = 1;
            as.item_size = sizeof(number);
            as.item_align = alignof(uint64_t);

            ff_value value = ff_value_new_data_array(as);
            ff_dict_set(&source, sv("number"), &value);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            const ff_ivalue* number_value = ff_idict_get(&dict, sv("number"));
            ff_array_span found = ff_ivalue_as_data(number_value, &dict);

            Assert::AreEqual((size_t)0, (size_t)number_value->data.offset % alignof(uint64_t));
            Assert::IsTrue(is_aligned(found.data, alignof(uint64_t)));
            Assert::AreEqual(number, *(const uint64_t*)found.data);

            ff_arena_destroy(&arena);
        }

        // ====================================================================
        // Arrays
        // ====================================================================
        TEST_METHOD(array_value_round_trips)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value items[3] =
            {
                ff_value_new_int32(10),
                ff_value_new_float64(2.5),
                ff_value_new_boolean(true),
            };

            ff_value_span items_span{ items, 3 };
            ff_value value = ff_value_new_array(items_span);
            ff_dict_set(&source, sv("list"), &value);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            ff_ivalue_span found = ff_ivalue_as_array(ff_idict_get(&dict, sv("list")), &dict);

            Assert::AreEqual((size_t)3, found.count);
            Assert::IsTrue(is_aligned(found.data, alignof(ff_ivalue)));
            Assert::AreEqual(10, found.data[0].i32);
            Assert::AreEqual(2.5, found.data[1].f64);
            Assert::IsTrue(found.data[2].b);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(empty_array_value)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value_span items_span{ nullptr, 0 };
            ff_value value = ff_value_new_array(items_span);
            ff_dict_set(&source, sv("list"), &value);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            ff_ivalue_span found = ff_ivalue_as_array(ff_idict_get(&dict, sv("list")), &dict);

            Assert::AreEqual((size_t)0, (size_t)found.count);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(array_of_strings_resolves_against_the_owning_dict)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value items[2] =
            {
                ff_value_new_string(sv("first")),
                ff_value_new_string(sv("second")),
            };

            ff_value_span items_span{ items, 2 };
            ff_value value = ff_value_new_array(items_span);
            ff_dict_set(&source, sv("list"), &value);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            ff_ivalue_span found = ff_ivalue_as_array(ff_idict_get(&dict, sv("list")), &dict);

            // Array items are not a block of their own, so their offsets are relative to this dict.
            ff_string_view first = ff_ivalue_as_string(found.data + 0, &dict);
            ff_string_view second = ff_ivalue_as_string(found.data + 1, &dict);

            Assert::AreEqual((size_t)5, first.count);
            Assert::IsTrue(memcmp(first.data, "first", 5) == 0);
            Assert::AreEqual((size_t)6, second.count);
            Assert::IsTrue(memcmp(second.data, "second", 6) == 0);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(nested_arrays_round_trip)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value inner_items[2] = { ff_value_new_int32(1), ff_value_new_int32(2) };
            ff_value_span inner_span{ inner_items, 2 };

            ff_value outer_items[2] =
            {
                ff_value_new_array(inner_span),
                ff_value_new_string(sv("tail")),
            };

            ff_value_span outer_span{ outer_items, 2 };
            ff_value value = ff_value_new_array(outer_span);
            ff_dict_set(&source, sv("list"), &value);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            ff_ivalue_span outer = ff_ivalue_as_array(ff_idict_get(&dict, sv("list")), &dict);
            ff_ivalue_span inner = ff_ivalue_as_array(outer.data + 0, &dict);

            Assert::AreEqual((size_t)2, inner.count);
            Assert::AreEqual(1, inner.data[0].i32);
            Assert::AreEqual(2, inner.data[1].i32);
            Assert::AreEqual((size_t)4, ff_ivalue_as_string(outer.data + 1, &dict).count);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(array_slice_metadata_describes_ivalues)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value items[3] = { ff_value_new_int32(1), ff_value_new_int32(2), ff_value_new_int32(3) };
            ff_value_span items_span{ items, 3 };
            ff_value value = ff_value_new_array(items_span);
            ff_dict_set(&source, sv("list"), &value);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            const ff_ivalue* found = ff_idict_get(&dict, sv("list"));

            Assert::AreEqual((size_t)3, (size_t)found->data.count);
            Assert::AreEqual((size_t)sizeof(ff_ivalue), (size_t)found->data.item_size);
            Assert::AreEqual((size_t)alignof(ff_ivalue), (size_t)found->data.item_align);
            Assert::AreEqual(data_start_of(count_of(&dict)) + 3 * sizeof(ff_ivalue), size_of(&dict));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(array_of_data_keeps_each_item_aligned)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            double first[2] = { 1.5, 2.5 };
            alignas(16) uint8_t second[16] = { 3 };

            ff_array_span first_span{};
            first_span.data = first;
            first_span.count = 2;
            first_span.item_size = sizeof(double);
            first_span.item_align = alignof(double);

            ff_array_span second_span{};
            second_span.data = second;
            second_span.count = 1;
            second_span.item_size = 16;
            second_span.item_align = 16;

            ff_value items[3] =
            {
                ff_value_new_string(sv("odd")),
                ff_value_new_data_array(first_span),
                ff_value_new_data_array(second_span),
            };

            ff_value_span items_span{ items, 3 };
            ff_value value = ff_value_new_array(items_span);
            ff_dict_set(&source, sv("list"), &value);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            ff_ivalue_span found = ff_ivalue_as_array(ff_idict_get(&dict, sv("list")), &dict);
            ff_array_span doubles = ff_ivalue_as_data(found.data + 1, &dict);
            ff_array_span wide = ff_ivalue_as_data(found.data + 2, &dict);

            Assert::IsTrue(is_aligned(dict.data, 16));
            Assert::IsTrue(is_aligned(doubles.data, alignof(double)));
            Assert::IsTrue(is_aligned(wide.data, 16));
            Assert::AreEqual(2.5, ((const double*)doubles.data)[1]);
            Assert::AreEqual((uint8_t)3, *(const uint8_t*)wide.data);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(array_of_nested_dicts_round_trips)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            const int count = 4;
            ff_dict inner[count]{};
            char texts[count][32];

            ff_value items[count];

            for (int i = 0; i < count; i++)
            {
                ff_dict_init(&inner[i], &arena);

                sprintf_s(texts[i], "child %d text", i);
                ff_value text = ff_value_new_string(sv(texts[i]));
                ff_value number = ff_value_new_int32(i * 3);
                ff_dict_set(&inner[i], sv("text"), &text);
                ff_dict_set(&inner[i], sv("number"), &number);

                items[i] = ff_value_new_dict(&inner[i]);
            }

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value_span items_span{ items, count };
            ff_value list = ff_value_new_array(items_span);
            ff_dict_set(&source, sv("list"), &list);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            ff_ivalue_span found = ff_ivalue_as_array(ff_idict_get(&dict, sv("list")), &dict);
            Assert::AreEqual((size_t)count, found.count);

            const uint8_t* previous_end = nullptr;

            for (int i = 0; i < count; i++)
            {
                // Each item is its own block, so it resolves against itself and not the outer dict.
                ff_idict item_dict = ff_ivalue_as_dict(found.data + i, &dict);
                ff_string_view text = ff_ivalue_as_string(ff_idict_get(&item_dict, sv("text")), &item_dict);

                Assert::AreEqual((size_t)2, count_of(&item_dict));
                Assert::AreEqual(i * 3, ff_idict_get(&item_dict, sv("number"))->i32);
                Assert::AreEqual(strlen(texts[i]), text.count);
                Assert::IsTrue(memcmp(text.data, texts[i], text.count) == 0);

                // Sibling blocks never overlap.
                const uint8_t* start = (const uint8_t*)item_dict.data;
                Assert::IsTrue(start >= previous_end);
                previous_end = start + size_of(&item_dict);
            }

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(large_array_survives_block_relocation)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            const int count = 2000;
            char(*texts)[32] = (char(*)[32])malloc(count * 32);
            ff_value* items = (ff_value*)malloc(count * sizeof(ff_value));
            Assert::IsNotNull(texts);
            Assert::IsNotNull(items);

            for (int i = 0; i < count; i++)
            {
                sprintf_s(texts[i], 32, "item %d of the big array", i);
                items[i] = ff_value_new_string(sv(texts[i]));
            }

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value_span items_span{ items, count };
            ff_value list = ff_value_new_array(items_span);
            ff_dict_set(&source, sv("list"), &list);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            // Writing item slots while the block keeps growing under them must not lose any of them.
            ff_ivalue_span found = ff_ivalue_as_array(ff_idict_get(&dict, sv("list")), &dict);
            Assert::AreEqual((size_t)count, found.count);

            for (int i = 0; i < count; i++)
            {
                ff_string_view text = ff_ivalue_as_string(found.data + i, &dict);

                Assert::AreEqual(strlen(texts[i]), text.count);
                Assert::IsTrue(memcmp(text.data, texts[i], text.count) == 0);
            }

            free(items);
            free(texts);
            ff_arena_destroy(&arena);
        }

        // ====================================================================
        // Nested dicts
        // ====================================================================
        TEST_METHOD(nested_dict_round_trips)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict inner{};
            ff_dict_init(&inner, &arena);

            ff_value inner_number = ff_value_new_int32(99);
            ff_value inner_text = ff_value_new_string(sv("deep"));
            ff_dict_set(&inner, sv("number"), &inner_number);
            ff_dict_set(&inner, sv("text"), &inner_text);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value nested = ff_value_new_dict(&inner);
            ff_value sibling = ff_value_new_int32(1);
            ff_dict_set(&source, sv("child"), &nested);
            ff_dict_set(&source, sv("sibling"), &sibling);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            const ff_ivalue* child = ff_idict_get(&dict, sv("child"));
            Assert::IsTrue(ff_value_type_dict == child->type);

            ff_idict child_dict = ff_ivalue_as_dict(child, &dict);

            Assert::AreEqual((size_t)2, count_of(&child_dict));
            Assert::AreEqual(99, ff_idict_get(&child_dict, sv("number"))->i32);

            // The nested string resolves against the nested block, not the outer one.
            ff_string_view text = ff_ivalue_as_string(ff_idict_get(&child_dict, sv("text")), &child_dict);
            Assert::AreEqual((size_t)4, text.count);
            Assert::IsTrue(memcmp(text.data, "deep", 4) == 0);

            Assert::AreEqual(1, ff_idict_get(&dict, sv("sibling"))->i32);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(nested_dict_is_a_self_contained_block)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict inner{};
            ff_dict_init(&inner, &arena);

            ff_value inner_value = ff_value_new_string(sv("payload"));
            ff_dict_set(&inner, sv("text"), &inner_value);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value nested = ff_value_new_dict(&inner);
            ff_dict_set(&source, sv("child"), &nested);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            const ff_ivalue* child = ff_idict_get(&dict, sv("child"));
            ff_idict child_dict = ff_ivalue_as_dict(child, &dict);

            // The nested block carries its own count and size, so the value only needs an offset.
            Assert::AreEqual((size_t)0, (size_t)child->data.count);
            Assert::AreEqual((size_t)0, (size_t)child->data.item_size);
            Assert::AreEqual((size_t)1, count_of(&child_dict));
            Assert::AreEqual(data_start_of(1) + 7, size_of(&child_dict));
            Assert::IsTrue(is_aligned(child_dict.data, alignof(uint64_t)));

            // The nested block sits entirely inside the outer block.
            const uint8_t* start = (const uint8_t*)child_dict.data;
            Assert::IsTrue(start >= data_of(dict));
            Assert::IsTrue(start + size_of(&child_dict) <= (const uint8_t*)dict.data + size_of(&dict));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(empty_nested_dict)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict inner{};
            ff_dict_init(&inner, &arena);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value nested = ff_value_new_dict(&inner);
            ff_dict_set(&source, sv("child"), &nested);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            ff_idict child_dict = ff_ivalue_as_dict(ff_idict_get(&dict, sv("child")), &dict);

            Assert::AreEqual((size_t)0, count_of(&child_dict));
            Assert::AreEqual(block_header_size, size_of(&child_dict));
            Assert::IsNull(ff_idict_get(&child_dict, sv("anything")));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(null_nested_dict_becomes_empty)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value nested = ff_value_new_dict(nullptr);
            ff_dict_set(&source, sv("child"), &nested);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            ff_idict child_dict = ff_ivalue_as_dict(ff_idict_get(&dict, sv("child")), &dict);

            Assert::AreEqual((size_t)0, count_of(&child_dict));
            Assert::IsNull(ff_idict_get(&child_dict, sv("anything")));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(deeply_nested_dicts_round_trip)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            const int depth = 8;
            ff_dict levels[depth]{};
            char names[depth][32];
            char key[32];

            for (int i = depth - 1; i >= 0; i--)
            {
                ff_dict_init(&levels[i], &arena);

                // Each name needs its own buffer: a string value points at the caller's bytes.
                sprintf_s(names[i], "level%d", i);
                ff_value marker = ff_value_new_string(sv(names[i]));
                ff_dict_set(&levels[i], sv("name"), &marker);

                if (i + 1 < depth)
                {
                    ff_value child = ff_value_new_dict(&levels[i + 1]);
                    ff_dict_set(&levels[i], sv("child"), &child);
                }
            }

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &levels[0]);

            ff_idict current = dict;
            for (int i = 0; i < depth; i++)
            {
                sprintf_s(key, "level%d", i);
                ff_string_view name = ff_ivalue_as_string(ff_idict_get(&current, sv("name")), &current);

                Assert::AreEqual(strlen(key), name.count);
                Assert::IsTrue(memcmp(name.data, key, name.count) == 0);

                const ff_ivalue* child = ff_idict_get(&current, sv("child"));
                if (i + 1 < depth)
                {
                    Assert::IsNotNull(child);
                    current = ff_ivalue_as_dict(child, &current);
                }
                else
                {
                    Assert::IsNull(child);
                }
            }

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(dict_inside_array_round_trips)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict inner{};
            ff_dict_init(&inner, &arena);

            ff_value inner_value = ff_value_new_string(sv("in array"));
            ff_dict_set(&inner, sv("text"), &inner_value);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value items[2] =
            {
                ff_value_new_dict(&inner),
                ff_value_new_int32(7),
            };

            ff_value_span items_span{ items, 2 };
            ff_value list = ff_value_new_array(items_span);
            ff_dict_set(&source, sv("list"), &list);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            ff_ivalue_span found = ff_ivalue_as_array(ff_idict_get(&dict, sv("list")), &dict);
            ff_idict item_dict = ff_ivalue_as_dict(found.data + 0, &dict);

            Assert::AreEqual((size_t)1, count_of(&item_dict));
            Assert::AreEqual((size_t)8, ff_ivalue_as_string(ff_idict_get(&item_dict, sv("text")), &item_dict).count);
            Assert::AreEqual(7, found.data[1].i32);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(array_inside_nested_dict_round_trips)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict inner{};
            ff_dict_init(&inner, &arena);

            ff_value items[2] = { ff_value_new_string(sv("a")), ff_value_new_int32(2) };
            ff_value_span items_span{ items, 2 };
            ff_value list = ff_value_new_array(items_span);
            ff_dict_set(&inner, sv("list"), &list);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value nested = ff_value_new_dict(&inner);
            ff_dict_set(&source, sv("child"), &nested);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            ff_idict child_dict = ff_ivalue_as_dict(ff_idict_get(&dict, sv("child")), &dict);
            ff_ivalue_span found = ff_ivalue_as_array(ff_idict_get(&child_dict, sv("list")), &child_dict);

            // Everything reachable from the nested dict resolves against the nested block.
            Assert::AreEqual((size_t)2, found.count);
            Assert::AreEqual((size_t)1, ff_ivalue_as_string(found.data + 0, &child_dict).count);
            Assert::IsTrue(memcmp(ff_ivalue_as_string(found.data + 0, &child_dict).data, "a", 1) == 0);
            Assert::AreEqual(2, found.data[1].i32);

            ff_arena_destroy(&arena);
        }

        // ====================================================================
        // Layout
        // ====================================================================
        TEST_METHOD(nested_dict_block_is_padded_to_its_own_alignment)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict inner{};
            ff_dict_init(&inner, &arena);

            ff_value inner_value = ff_value_new_int32(1);
            ff_dict_set(&inner, sv("number"), &inner_value);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            // One loose byte first, so the nested block has to be pushed to the next aligned offset.
            ff_value text = ff_value_new_string(sv("x"));
            ff_value nested = ff_value_new_dict(&inner);
            ff_dict_set(&source, sv("text"), &text);
            ff_dict_set(&source, sv("child"), &nested);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            const ff_ivalue* child = ff_idict_get(&dict, sv("child"));
            ff_idict child_dict = ff_ivalue_as_dict(child, &dict);

            // Every block sits on the format's max alignment, so the loose byte before it forces
            // the nested block all the way to the next boundary.
            Assert::AreEqual((size_t)ff_idict_max_align, (size_t)child->data.offset);
            Assert::IsTrue(is_aligned(child_dict.data, ff_idict_max_align));
            Assert::AreEqual(1, ff_idict_get(&child_dict, sv("number"))->i32);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(nested_dict_size_excludes_trailing_siblings)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict inner{};
            ff_dict_init(&inner, &arena);

            ff_value inner_value = ff_value_new_string(sv("payload"));
            ff_dict_set(&inner, sv("text"), &inner_value);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            // The sibling's bytes land after the nested block and must not be counted as part of it.
            ff_value nested = ff_value_new_dict(&inner);
            ff_value sibling = ff_value_new_string(sv("a much longer sibling string"));
            ff_dict_set(&source, sv("child"), &nested);
            ff_dict_set(&source, sv("sibling"), &sibling);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            ff_idict child_dict = ff_ivalue_as_dict(ff_idict_get(&dict, sv("child")), &dict);

            Assert::AreEqual(data_start_of(1) + 7, size_of(&child_dict));
            Assert::AreEqual((size_t)28, ff_ivalue_as_string(ff_idict_get(&dict, sv("sibling")), &dict).count);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(sibling_nested_dicts_are_independent_blocks)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict first{};
            ff_dict second{};
            ff_dict_init(&first, &arena);
            ff_dict_init(&second, &arena);

            ff_value first_text = ff_value_new_string(sv("aaa"));
            ff_dict_set(&first, sv("text"), &first_text);

            ff_value second_text = ff_value_new_string(sv("bbbbbbbbb"));
            ff_value second_number = ff_value_new_int32(42);
            ff_dict_set(&second, sv("text"), &second_text);
            ff_dict_set(&second, sv("number"), &second_number);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value a = ff_value_new_dict(&first);
            ff_value b = ff_value_new_dict(&second);
            ff_dict_set(&source, sv("a"), &a);
            ff_dict_set(&source, sv("b"), &b);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            ff_idict a_dict = ff_ivalue_as_dict(ff_idict_get(&dict, sv("a")), &dict);
            ff_idict b_dict = ff_ivalue_as_dict(ff_idict_get(&dict, sv("b")), &dict);

            Assert::AreEqual((size_t)1, count_of(&a_dict));
            Assert::AreEqual((size_t)2, count_of(&b_dict));
            Assert::AreEqual(data_start_of(1) + 3, size_of(&a_dict));
            Assert::AreEqual(data_start_of(2) + 9, size_of(&b_dict));

            // The same key in each block resolves to that block's own bytes.
            Assert::IsTrue(memcmp(ff_ivalue_as_string(ff_idict_get(&a_dict, sv("text")), &a_dict).data, "aaa", 3) == 0);
            Assert::IsTrue(memcmp(ff_ivalue_as_string(ff_idict_get(&b_dict, sv("text")), &b_dict).data, "bbbbbbbbb", 9) == 0);
            Assert::IsNull(ff_idict_get(&a_dict, sv("number")));

            const uint8_t* a_start = (const uint8_t*)a_dict.data;
            const uint8_t* b_start = (const uint8_t*)b_dict.data;
            Assert::IsTrue(b_start >= a_start + size_of(&a_dict));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(over_aligned_data_inside_a_nested_dict_stays_aligned)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict inner{};
            ff_dict_init(&inner, &arena);

            alignas(64) uint8_t wide[64];
            memset(wide, 0x5A, sizeof(wide));

            ff_array_span as{};
            as.data = wide;
            as.count = 1;
            as.item_size = 64;
            as.item_align = 64;

            // A stray byte inside the nested block too, so nothing lines up by accident.
            ff_value inner_text = ff_value_new_string(sv("z"));
            ff_value inner_wide = ff_value_new_data_array(as);
            ff_dict_set(&inner, sv("text"), &inner_text);
            ff_dict_set(&inner, sv("wide"), &inner_wide);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value outer_text = ff_value_new_string(sv("pad"));
            ff_value nested = ff_value_new_dict(&inner);
            ff_dict_set(&source, sv("pad"), &outer_text);
            ff_dict_set(&source, sv("child"), &nested);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            ff_idict child_dict = ff_ivalue_as_dict(ff_idict_get(&dict, sv("child")), &dict);
            ff_array_span found = ff_ivalue_as_data(ff_idict_get(&child_dict, sv("wide")), &child_dict);

            // Padding is computed on absolute offsets, so the nested item is still 64 byte aligned
            // even though its recorded offset is relative to the nested block.
            Assert::IsTrue(is_aligned(dict.data, 64));
            Assert::IsTrue(is_aligned(found.data, 64));
            Assert::AreEqual((size_t)64, (size_t)found.item_align);
            Assert::IsTrue(memcmp(found.data, wide, sizeof(wide)) == 0);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(nested_dict_inside_array_inside_nested_dict)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict leaf{};
            ff_dict_init(&leaf, &arena);

            ff_value leaf_text = ff_value_new_string(sv("found me"));
            ff_dict_set(&leaf, sv("leaf"), &leaf_text);

            ff_dict middle{};
            ff_dict_init(&middle, &arena);

            ff_value items[2] = { ff_value_new_dict(&leaf), ff_value_new_string(sv("tail")) };
            ff_value_span items_span{ items, 2 };
            ff_value list = ff_value_new_array(items_span);
            ff_dict_set(&middle, sv("list"), &list);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value nested = ff_value_new_dict(&middle);
            ff_dict_set(&source, sv("child"), &nested);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            ff_idict middle_dict = ff_ivalue_as_dict(ff_idict_get(&dict, sv("child")), &dict);
            ff_ivalue_span found = ff_ivalue_as_array(ff_idict_get(&middle_dict, sv("list")), &middle_dict);

            // The array belongs to the middle block; the item is a block of its own again.
            ff_idict leaf_dict = ff_ivalue_as_dict(found.data + 0, &middle_dict);
            ff_string_view text = ff_ivalue_as_string(ff_idict_get(&leaf_dict, sv("leaf")), &leaf_dict);

            Assert::AreEqual((size_t)2, found.count);
            Assert::AreEqual((size_t)8, text.count);
            Assert::IsTrue(memcmp(text.data, "found me", 8) == 0);
            Assert::AreEqual((size_t)4, ff_ivalue_as_string(found.data + 1, &middle_dict).count);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(nested_dict_holding_every_kind_of_value)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict grandchild{};
            ff_dict_init(&grandchild, &arena);

            ff_value grandchild_number = ff_value_new_int64(-77);
            ff_dict_set(&grandchild, sv("number"), &grandchild_number);

            ff_dict inner{};
            ff_dict_init(&inner, &arena);

            double numbers[2] = { 0.25, 0.75 };
            ff_array_span as{};
            as.data = numbers;
            as.count = 2;
            as.item_size = sizeof(double);
            as.item_align = alignof(double);

            ff_value array_items[2] = { ff_value_new_boolean(true), ff_value_new_string(sv("odd")) };
            ff_value_span array_span{ array_items, 2 };

            ff_value inner_scalar = ff_value_new_int32(3);
            ff_value inner_string = ff_value_new_string(sv("inner string"));
            ff_value inner_data = ff_value_new_data_array(as);
            ff_value inner_array = ff_value_new_array(array_span);
            ff_value inner_dict = ff_value_new_dict(&grandchild);

            ff_dict_set(&inner, sv("scalar"), &inner_scalar);
            ff_dict_set(&inner, sv("string"), &inner_string);
            ff_dict_set(&inner, sv("data"), &inner_data);
            ff_dict_set(&inner, sv("array"), &inner_array);
            ff_dict_set(&inner, sv("dict"), &inner_dict);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value nested = ff_value_new_dict(&inner);
            ff_dict_set(&source, sv("child"), &nested);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            ff_idict child_dict = ff_ivalue_as_dict(ff_idict_get(&dict, sv("child")), &dict);
            Assert::AreEqual((size_t)5, count_of(&child_dict));
            Assert::AreEqual(3, ff_idict_get(&child_dict, sv("scalar"))->i32);

            ff_string_view text = ff_ivalue_as_string(ff_idict_get(&child_dict, sv("string")), &child_dict);
            Assert::AreEqual((size_t)12, text.count);
            Assert::IsTrue(memcmp(text.data, "inner string", 12) == 0);

            ff_array_span data = ff_ivalue_as_data(ff_idict_get(&child_dict, sv("data")), &child_dict);
            Assert::IsTrue(is_aligned(data.data, alignof(double)));
            Assert::AreEqual(0.75, ((const double*)data.data)[1]);

            ff_ivalue_span array = ff_ivalue_as_array(ff_idict_get(&child_dict, sv("array")), &child_dict);
            Assert::AreEqual((size_t)2, array.count);
            Assert::IsTrue(array.data[0].b);
            Assert::AreEqual((size_t)3, ff_ivalue_as_string(array.data + 1, &child_dict).count);

            ff_idict grandchild_dict = ff_ivalue_as_dict(ff_idict_get(&child_dict, sv("dict")), &child_dict);
            Assert::AreEqual((size_t)1, count_of(&grandchild_dict));
            Assert::AreEqual((int64_t)-77, ff_idict_get(&grandchild_dict, sv("number"))->i64);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(sibling_nested_dicts_with_their_own_children)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            const int siblings = 3;
            const int grandkids = 2;

            ff_dict grand[siblings][grandkids]{};
            ff_dict child[siblings]{};
            char grand_names[siblings][grandkids][32];
            char child_names[siblings][32];
            char key[32];

            for (int s = 0; s < siblings; s++)
            {
                ff_dict_init(&child[s], &arena);

                sprintf_s(child_names[s], "child %d", s);
                ff_value child_name = ff_value_new_string(sv(child_names[s]));
                ff_dict_set(&child[s], sv("name"), &child_name);

                for (int g = 0; g < grandkids; g++)
                {
                    ff_dict_init(&grand[s][g], &arena);

                    sprintf_s(grand_names[s][g], "grand %d.%d", s, g);
                    ff_value grand_name = ff_value_new_string(sv(grand_names[s][g]));
                    ff_value depth = ff_value_new_int32(s * 10 + g);
                    ff_dict_set(&grand[s][g], sv("name"), &grand_name);
                    ff_dict_set(&grand[s][g], sv("depth"), &depth);

                    sprintf_s(key, "kid%d", g);
                    ff_value kid = ff_value_new_dict(&grand[s][g]);
                    ff_dict_set(&child[s], sv(key), &kid);
                }
            }

            ff_dict source{};
            ff_dict_init(&source, &arena);

            for (int s = 0; s < siblings; s++)
            {
                sprintf_s(key, "child%d", s);
                ff_value value = ff_value_new_dict(&child[s]);
                ff_dict_set(&source, sv(key), &value);
            }

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            const uint8_t* child_starts[siblings]{};
            const uint8_t* child_ends[siblings]{};

            for (int s = 0; s < siblings; s++)
            {
                sprintf_s(key, "child%d", s);
                ff_idict child_dict = ff_ivalue_as_dict(ff_idict_get(&dict, sv(key)), &dict);
                ff_string_view child_name = ff_ivalue_as_string(ff_idict_get(&child_dict, sv("name")), &child_dict);

                Assert::AreEqual((size_t)(1 + grandkids), count_of(&child_dict));
                Assert::AreEqual(strlen(child_names[s]), child_name.count);
                Assert::IsTrue(memcmp(child_name.data, child_names[s], child_name.count) == 0);

                const uint8_t* child_start = (const uint8_t*)child_dict.data;
                child_starts[s] = child_start;
                child_ends[s] = child_start + size_of(&child_dict);

                for (int g = 0; g < grandkids; g++)
                {
                    sprintf_s(key, "kid%d", g);
                    ff_idict grand_dict = ff_ivalue_as_dict(ff_idict_get(&child_dict, sv(key)), &child_dict);
                    ff_string_view grand_name = ff_ivalue_as_string(ff_idict_get(&grand_dict, sv("name")), &grand_dict);

                    Assert::AreEqual(s * 10 + g, ff_idict_get(&grand_dict, sv("depth"))->i32);
                    Assert::AreEqual(strlen(grand_names[s][g]), grand_name.count);
                    Assert::IsTrue(memcmp(grand_name.data, grand_names[s][g], grand_name.count) == 0);

                    // The grandchild is contained by its own parent, not by an uncle.
                    const uint8_t* grand_start = (const uint8_t*)grand_dict.data;
                    Assert::IsTrue(grand_start >= child_start);
                    Assert::IsTrue(grand_start + size_of(&grand_dict) <= child_ends[s]);
                }
            }

            // Siblings are laid out in key order rather than in the order they were added, so which
            // one comes first is not fixed. What has to hold is that no two subtrees overlap.
            for (int a = 0; a < siblings; a++)
            {
                for (int b = a + 1; b < siblings; b++)
                {
                    Assert::IsTrue(child_ends[a] <= child_starts[b] || child_ends[b] <= child_starts[a]);
                }
            }

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(empty_nested_dict_between_populated_siblings)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict first{};
            ff_dict middle{};
            ff_dict last{};
            ff_dict_init(&first, &arena);
            ff_dict_init(&middle, &arena);
            ff_dict_init(&last, &arena);

            ff_value first_text = ff_value_new_string(sv("aaa"));
            ff_value last_text = ff_value_new_string(sv("ccc"));
            ff_dict_set(&first, sv("text"), &first_text);
            ff_dict_set(&last, sv("text"), &last_text);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value a = ff_value_new_dict(&first);
            ff_value b = ff_value_new_dict(&middle);
            ff_value c = ff_value_new_dict(&last);
            ff_dict_set(&source, sv("a"), &a);
            ff_dict_set(&source, sv("b"), &b);
            ff_dict_set(&source, sv("c"), &c);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            ff_idict a_dict = ff_ivalue_as_dict(ff_idict_get(&dict, sv("a")), &dict);
            ff_idict b_dict = ff_ivalue_as_dict(ff_idict_get(&dict, sv("b")), &dict);
            ff_idict c_dict = ff_ivalue_as_dict(ff_idict_get(&dict, sv("c")), &dict);

            // A zero sized block in the middle must not swallow or shift its neighbours.
            Assert::AreEqual((size_t)0, count_of(&b_dict));
            Assert::AreEqual(block_header_size, size_of(&b_dict));
            Assert::IsNull(ff_idict_get(&b_dict, sv("text")));
            Assert::IsTrue(memcmp(ff_ivalue_as_string(ff_idict_get(&a_dict, sv("text")), &a_dict).data, "aaa", 3) == 0);
            Assert::IsTrue(memcmp(ff_ivalue_as_string(ff_idict_get(&c_dict, sv("text")), &c_dict).data, "ccc", 3) == 0);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(cleared_source_entries_are_not_copied)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict inner{};
            ff_dict_init(&inner, &arena);

            ff_value keep = ff_value_new_string(sv("keep"));
            ff_value drop = ff_value_new_string(sv("this one goes away"));
            ff_dict_set(&inner, sv("keep"), &keep);
            ff_dict_set(&inner, sv("drop"), &drop);

            Assert::IsTrue(ff_dict_clear(&inner, sv("drop")));

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value nested = ff_value_new_dict(&inner);
            ff_dict_set(&source, sv("child"), &nested);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            const ff_ivalue* child = ff_idict_get(&dict, sv("child"));
            ff_idict child_dict = ff_ivalue_as_dict(child, &dict);

            // Clearing compacts the source, so the block carries neither the key nor the bytes.
            Assert::AreEqual((size_t)1, count_of(&child_dict));
            Assert::AreEqual(data_start_of(1) + 4, size_of(&child_dict));
            Assert::IsNull(ff_idict_get(&child_dict, sv("drop")));
            Assert::IsTrue(memcmp(ff_ivalue_as_string(ff_idict_get(&child_dict, sv("keep")), &child_dict).data, "keep", 4) == 0);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(array_of_dicts_each_holding_an_array_of_dicts)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            const int outer = 3;
            const int inner = 2;

            ff_dict leaf[outer][inner]{};
            ff_dict middle[outer]{};
            char texts[outer][inner][32];
            ff_value leaf_values[outer][inner];
            ff_value middle_values[outer];

            for (int o = 0; o < outer; o++)
            {
                for (int i = 0; i < inner; i++)
                {
                    ff_dict_init(&leaf[o][i], &arena);

                    sprintf_s(texts[o][i], "leaf %d.%d", o, i);
                    ff_value text = ff_value_new_string(sv(texts[o][i]));
                    ff_dict_set(&leaf[o][i], sv("text"), &text);

                    leaf_values[o][i] = ff_value_new_dict(&leaf[o][i]);
                }

                ff_dict_init(&middle[o], &arena);

                ff_value_span leaf_span{ leaf_values[o], inner };
                ff_value list = ff_value_new_array(leaf_span);
                ff_dict_set(&middle[o], sv("list"), &list);

                middle_values[o] = ff_value_new_dict(&middle[o]);
            }

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value_span middle_span{ middle_values, outer };
            ff_value list = ff_value_new_array(middle_span);
            ff_dict_set(&source, sv("list"), &list);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            ff_ivalue_span found = ff_ivalue_as_array(ff_idict_get(&dict, sv("list")), &dict);
            Assert::AreEqual((size_t)outer, found.count);

            for (int o = 0; o < outer; o++)
            {
                // Array items resolve against the owning dict, but each dict item is a block again.
                ff_idict middle_dict = ff_ivalue_as_dict(found.data + o, &dict);
                ff_ivalue_span leaf_span = ff_ivalue_as_array(ff_idict_get(&middle_dict, sv("list")), &middle_dict);

                Assert::AreEqual((size_t)inner, leaf_span.count);

                for (int i = 0; i < inner; i++)
                {
                    ff_idict leaf_dict = ff_ivalue_as_dict(leaf_span.data + i, &middle_dict);
                    ff_string_view text = ff_ivalue_as_string(ff_idict_get(&leaf_dict, sv("text")), &leaf_dict);

                    Assert::AreEqual(strlen(texts[o][i]), text.count);
                    Assert::IsTrue(memcmp(text.data, texts[o][i], text.count) == 0);
                }
            }

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(array_of_arrays_of_dicts)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            const int outer = 2;
            const int inner = 2;

            ff_dict leaf[outer][inner]{};
            char texts[outer][inner][32];
            ff_value leaf_values[outer][inner];
            ff_value inner_arrays[outer];

            for (int o = 0; o < outer; o++)
            {
                for (int i = 0; i < inner; i++)
                {
                    ff_dict_init(&leaf[o][i], &arena);

                    sprintf_s(texts[o][i], "cell %d.%d", o, i);
                    ff_value text = ff_value_new_string(sv(texts[o][i]));
                    ff_value number = ff_value_new_int32(o * 100 + i);
                    ff_dict_set(&leaf[o][i], sv("text"), &text);
                    ff_dict_set(&leaf[o][i], sv("number"), &number);

                    leaf_values[o][i] = ff_value_new_dict(&leaf[o][i]);
                }

                ff_value_span leaf_span{ leaf_values[o], inner };
                inner_arrays[o] = ff_value_new_array(leaf_span);
            }

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value_span outer_span{ inner_arrays, outer };
            ff_value list = ff_value_new_array(outer_span);
            ff_dict_set(&source, sv("grid"), &list);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            ff_ivalue_span rows = ff_ivalue_as_array(ff_idict_get(&dict, sv("grid")), &dict);
            Assert::AreEqual((size_t)outer, rows.count);

            for (int o = 0; o < outer; o++)
            {
                // Arrays never form a block, so even two levels down they resolve against this dict.
                ff_ivalue_span cells = ff_ivalue_as_array(rows.data + o, &dict);
                Assert::AreEqual((size_t)inner, cells.count);

                for (int i = 0; i < inner; i++)
                {
                    ff_idict cell = ff_ivalue_as_dict(cells.data + i, &dict);
                    ff_string_view text = ff_ivalue_as_string(ff_idict_get(&cell, sv("text")), &cell);

                    Assert::AreEqual(o * 100 + i, ff_idict_get(&cell, sv("number"))->i32);
                    Assert::AreEqual(strlen(texts[o][i]), text.count);
                    Assert::IsTrue(memcmp(text.data, texts[o][i], text.count) == 0);
                }
            }

            ff_arena_destroy(&arena);
        }

        // ====================================================================
        // Layout
        // ====================================================================
        // Walks every block in the tree and checks the two rules the layout promises: a block sits
        // on the max alignment, and so does the data section that follows its values.
        static void check_block_alignment(const ff_idict& dict, size_t depth)
        {
            Assert::IsTrue(depth < 32);
            Assert::IsTrue(is_aligned(dict.data, ff_idict_max_align));
            Assert::IsTrue(is_aligned(data_of(dict), ff_idict_max_align));

            const ff_ivalue* values = values_of(dict);

            for (size_t i = 0; i < count_of(&dict); i++)
            {
                if (values[i].type == ff_value_type_dict)
                {
                    ff_idict child = ff_ivalue_as_dict(values + i, &dict);
                    check_block_alignment(child, depth + 1);
                }
                else if (values[i].type == ff_value_type_array)
                {
                    ff_ivalue_span items = ff_ivalue_as_array(values + i, &dict);

                    for (size_t item = 0; item < items.count; item++)
                    {
                        if (items.data[item].type == ff_value_type_dict)
                        {
                            ff_idict child = ff_ivalue_as_dict(items.data + item, &dict);
                            check_block_alignment(child, depth + 1);
                        }
                    }
                }
            }
        }

        TEST_METHOD(every_block_and_data_section_sits_on_the_max_alignment)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            build_rich_source(&arena, &source);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            check_block_alignment(dict, 0);

            // Still true after the bytes have been through a save and a load.
            ff_span saved = ff_idict_save(&dict, &arena);

            ff_arena load_arena{};
            ff_arena_init_heap_global(&load_arena, 4096);
            ff_arena_alloc(&load_arena, 1, 1); // dirty it so nothing lines up by luck

            ff_idict loaded{};
            Assert::IsTrue(ff_idict_load(&loaded, &load_arena, saved));
            check_block_alignment(loaded, 0);

            ff_arena_destroy(&load_arena);
            ff_arena_destroy(&arena);
        }

        TEST_METHOD(simd_sized_data_is_aligned_wherever_it_lands)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            alignas(ff_idict_max_align) uint8_t wide[ff_idict_max_align * 2];
            memset(wide, 0xA5, sizeof(wide));

            ff_array_span as{};
            as.data = wide;
            as.count = 2;
            as.item_size = ff_idict_max_align;
            as.item_align = ff_idict_max_align;

            ff_dict inner{};
            ff_dict_init(&inner, &arena);

            // Odd sized junk before and after each wide value, so nothing can line up by accident.
            ff_value inner_a = ff_value_new_string(sv("q"));
            ff_value inner_wide = ff_value_new_data_array(as);
            ff_value inner_b = ff_value_new_string(sv("qqq"));
            ff_dict_set(&inner, sv("a"), &inner_a);
            ff_dict_set(&inner, sv("wide"), &inner_wide);
            ff_dict_set(&inner, sv("b"), &inner_b);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value outer_a = ff_value_new_string(sv("zz"));
            ff_value outer_wide = ff_value_new_data_array(as);
            ff_value nested = ff_value_new_dict(&inner);
            ff_dict_set(&source, sv("a"), &outer_a);
            ff_dict_set(&source, sv("wide"), &outer_wide);
            ff_dict_set(&source, sv("child"), &nested);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            ff_array_span outer_found = ff_ivalue_as_data(ff_idict_get(&dict, sv("wide")), &dict);
            Assert::IsTrue(is_aligned(outer_found.data, ff_idict_max_align));
            Assert::IsTrue(memcmp(outer_found.data, wide, sizeof(wide)) == 0);

            ff_idict child_dict = ff_ivalue_as_dict(ff_idict_get(&dict, sv("child")), &dict);
            ff_array_span inner_found = ff_ivalue_as_data(ff_idict_get(&child_dict, sv("wide")), &child_dict);
            Assert::IsTrue(is_aligned(inner_found.data, ff_idict_max_align));
            Assert::IsTrue(memcmp(inner_found.data, wide, sizeof(wide)) == 0);

            // Both items of each array are aligned, not just the first.
            Assert::IsTrue(is_aligned((const uint8_t*)inner_found.data + ff_idict_max_align, ff_idict_max_align));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(a_dict_is_nothing_but_a_pointer_to_its_block)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value number = ff_value_new_int32(7);
            ff_dict_set(&source, sv("number"), &number);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            // Everything a dict knows comes out of the block, so copying the struct copies nothing
            // but an address and the copy has to behave identically.
            Assert::AreEqual(sizeof(void*), sizeof(ff_idict));

            ff_idict copy = dict;
            Assert::AreEqual(count_of(&dict), count_of(&copy));
            Assert::AreEqual(size_of(&dict), size_of(&copy));
            Assert::AreEqual(7, ff_idict_get(&copy, sv("number"))->i32);

            // A dict that was never built answers as an empty one instead of crashing.
            ff_idict empty{};
            Assert::AreEqual((size_t)0, count_of(&empty));
            Assert::AreEqual((size_t)0, size_of(&empty));
            Assert::IsNull(ff_idict_get(&empty, sv("number")));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(keys_come_first_then_values_then_data)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value number = ff_value_new_int32(1);
            ff_value text = ff_value_new_string(sv("data"));
            ff_dict_set(&source, sv("number"), &number);
            ff_dict_set(&source, sv("text"), &text);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            Assert::AreEqual((size_t)2, count_of(&dict));
            Assert::IsTrue((const void*)keys_of(dict) == (const uint8_t*)dict.data + block_header_size);
            Assert::IsTrue((const uint8_t*)values_of(dict) == (const uint8_t*)keys_of(dict) + 2 * sizeof(uint64_t));

            // The data section starts on the format's max alignment rather than packed right behind
            // the values, so the first item in it is already aligned for anything up to that.
            Assert::IsTrue(data_of(dict) > (const uint8_t*)values_of(dict) + 2 * sizeof(ff_ivalue));
            Assert::IsTrue(is_aligned(data_of(dict), ff_idict_max_align));
            Assert::AreEqual(data_start_of(2) + 4, size_of(&dict));

            // Nothing but zeroes between the values and the data section.
            for (const uint8_t* pad = (const uint8_t*)values_of(dict) + 2 * sizeof(ff_ivalue); pad < data_of(dict); pad++)
            {
                Assert::AreEqual((uint8_t)0, *pad);
            }

            Assert::IsTrue(memcmp(data_of(dict), "data", 4) == 0);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(keys_match_the_source_hashes)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value a = ff_value_new_int32(1);
            ff_value b = ff_value_new_int32(2);
            ff_dict_set(&source, sv("a"), &a);
            ff_dict_set(&source, sv("b"), &b);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            Assert::AreEqual(ff_hash_string(sv("a")), keys_of(dict)[0]);
            Assert::AreEqual(ff_hash_string(sv("b")), keys_of(dict)[1]);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(scalars_only_block_has_no_data_section)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            for (int i = 0; i < 4; i++)
            {
                char key[32];
                sprintf_s(key, "key%d", i);
                ff_value value = ff_value_new_int32(i);
                ff_dict_set(&source, sv(key), &value);
            }

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            Assert::AreEqual(block_header_size + 4 * entry_size, size_of(&dict));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(block_is_a_single_allocation_in_the_caller_arena)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 64 * 1024);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value text = ff_value_new_string(sv("hello"));
            ff_dict_set(&source, sv("text"), &text);

            ff_arena_marker marker = ff_arena_mark(&arena);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            // The scratch arena holds the build, so the caller's arena only gets the finished block,
            // plus whatever padding it took to put that block on its required alignment.
            const uint8_t* block = (const uint8_t*)dict.data;
            Assert::IsTrue(is_aligned(block, ff_idict_max_align));
            Assert::IsTrue(block >= marker && (size_t)(block - marker) < ff_idict_max_align);
            Assert::AreEqual(size_of(&dict), (size_t)(ff_arena_mark(&arena) - block));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(immutable_copy_outlives_the_source)
        {
            ff_arena source_arena{};
            ff_arena_init_heap_global(&source_arena, 4096);

            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict inner{};
            ff_dict_init(&inner, &source_arena);

            ff_value inner_value = ff_value_new_string(sv("nested"));
            ff_dict_set(&inner, sv("text"), &inner_value);

            ff_dict source{};
            ff_dict_init(&source, &source_arena);

            ff_value nested = ff_value_new_dict(&inner);
            ff_value number = ff_value_new_int32(5);
            ff_dict_set(&source, sv("child"), &nested);
            ff_dict_set(&source, sv("number"), &number);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            ff_arena_destroy(&source_arena);

            Assert::AreEqual(5, ff_idict_get(&dict, sv("number"))->i32);

            ff_idict child_dict = ff_ivalue_as_dict(ff_idict_get(&dict, sv("child")), &dict);
            ff_string_view text = ff_ivalue_as_string(ff_idict_get(&child_dict, sv("text")), &child_dict);

            Assert::AreEqual((size_t)6, text.count);
            Assert::IsTrue(memcmp(text.data, "nested", 6) == 0);

            ff_arena_destroy(&arena);
        }

        // ====================================================================
        // Portability
        // ====================================================================

        // Builds a tree that touches every encoding: scalars, strings, over aligned data, arrays and
        // nested dicts two deep. Everything it points at is arena owned or a literal, so it stays
        // valid for as long as the arena does.
        static void build_rich_source(ff_arena* arena, ff_dict* source)
        {
            static alignas(64) const uint8_t wide[64] = { 1, 2, 3 };
            static const double numbers[3] = { 0.5, 1.5, 2.5 };

            ff_dict* grand = (ff_dict*)ff_arena_alloc(arena, sizeof(ff_dict), alignof(ff_dict));
            ff_dict_init(grand, arena);

            ff_value grand_text = ff_value_new_string(sv("grandchild"));
            ff_value grand_number = ff_value_new_int64(-9);
            ff_dict_set(grand, sv("text"), &grand_text);
            ff_dict_set(grand, sv("number"), &grand_number);

            ff_dict* child = (ff_dict*)ff_arena_alloc(arena, sizeof(ff_dict), alignof(ff_dict));
            ff_dict_init(child, arena);

            ff_value* items = (ff_value*)ff_arena_alloc(arena, 2 * sizeof(ff_value), alignof(ff_value));
            items[0] = ff_value_new_dict(grand);
            items[1] = ff_value_new_string(sv("in array"));

            ff_value_span items_span{ items, 2 };
            ff_value list = ff_value_new_array(items_span);
            ff_value flag = ff_value_new_boolean(true);
            ff_dict_set(child, sv("list"), &list);
            ff_dict_set(child, sv("flag"), &flag);

            ff_array_span wide_span{};
            wide_span.data = wide;
            wide_span.count = 1;
            wide_span.item_size = 64;
            wide_span.item_align = 64;

            ff_array_span number_span{};
            number_span.data = numbers;
            number_span.count = 3;
            number_span.item_size = sizeof(double);
            number_span.item_align = alignof(double);

            ff_dict_init(source, arena);

            ff_value pad = ff_value_new_string(sv("x"));
            ff_value blob = ff_value_new_data_array(wide_span);
            ff_value nums = ff_value_new_data_array(number_span);
            ff_value nested = ff_value_new_dict(child);
            ff_value number = ff_value_new_int32(7);

            ff_dict_set(source, sv("pad"), &pad);
            ff_dict_set(source, sv("blob"), &blob);
            ff_dict_set(source, sv("nums"), &nums);
            ff_dict_set(source, sv("child"), &nested);
            ff_dict_set(source, sv("number"), &number);
        }

        static void verify_rich_block(const ff_idict& dict)
        {
            Assert::AreEqual((size_t)5, count_of(&dict));
            Assert::AreEqual(7, ff_idict_get(&dict, sv("number"))->i32);
            Assert::AreEqual((size_t)1, ff_ivalue_as_string(ff_idict_get(&dict, sv("pad")), &dict).count);

            ff_array_span blob = ff_ivalue_as_data(ff_idict_get(&dict, sv("blob")), &dict);
            Assert::IsTrue(is_aligned(blob.data, 64));
            Assert::AreEqual((uint8_t)2, ((const uint8_t*)blob.data)[1]);

            ff_array_span nums = ff_ivalue_as_data(ff_idict_get(&dict, sv("nums")), &dict);
            Assert::IsTrue(is_aligned(nums.data, alignof(double)));
            Assert::AreEqual(2.5, ((const double*)nums.data)[2]);

            ff_idict child = ff_ivalue_as_dict(ff_idict_get(&dict, sv("child")), &dict);
            Assert::AreEqual((size_t)2, count_of(&child));
            Assert::IsTrue(ff_idict_get(&child, sv("flag"))->b);

            ff_ivalue_span list = ff_ivalue_as_array(ff_idict_get(&child, sv("list")), &child);
            Assert::AreEqual((size_t)2, list.count);
            Assert::AreEqual((size_t)8, ff_ivalue_as_string(list.data + 1, &child).count);

            ff_idict grand = ff_ivalue_as_dict(list.data + 0, &child);
            Assert::AreEqual((int64_t)-9, ff_idict_get(&grand, sv("number"))->i64);
            Assert::IsTrue(memcmp(ff_ivalue_as_string(ff_idict_get(&grand, sv("text")), &grand).data, "grandchild", 10) == 0);
        }

        TEST_METHOD(block_survives_being_moved_to_another_buffer)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            build_rich_source(&arena, &source);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            verify_rich_block(dict);

            // A block is only offsets, so a raw byte copy somewhere else must resolve identically.
            // This is what loading one back from disk amounts to.
            uint8_t* raw = (uint8_t*)malloc(size_of(&dict) + ff_idict_max_align);
            Assert::IsNotNull(raw);

            uint8_t* copy = (uint8_t*)(((uintptr_t)raw + ff_idict_max_align - 1) & ~(uintptr_t)(ff_idict_max_align - 1));
            memcpy(copy, dict.data, size_of(&dict));

            ff_idict moved{};
            moved.data = copy;

            Assert::IsTrue(moved.data != dict.data);
            verify_rich_block(moved);

            // The original stays valid too, so nothing was fixed up in place.
            verify_rich_block(dict);

            free(raw);
            ff_arena_destroy(&arena);
        }

        TEST_METHOD(block_bytes_do_not_depend_on_where_it_was_built)
        {
            ff_arena first_arena{};
            ff_arena second_arena{};
            ff_arena_init_heap_global(&first_arena, 4096);
            ff_arena_init_heap_global(&second_arena, 64 * 1024);

            ff_dict first_source{};
            ff_dict second_source{};
            build_rich_source(&first_arena, &first_source);
            build_rich_source(&second_arena, &second_source);

            ff_idict first{};
            ff_idict second{};
            ff_idict_init(&first, &first_arena, &first_source);
            ff_idict_init(&second, &second_arena, &second_source);

            // Two builds at different addresses: any address that leaked into the block, or any
            // uninitialized padding, would show up as a difference here.
            Assert::IsTrue(first.data != second.data);
            Assert::AreEqual(count_of(&first), count_of(&second));
            Assert::AreEqual(size_of(&first), size_of(&second));
            Assert::IsTrue(memcmp(first.data, second.data, size_of(&first)) == 0);

            ff_arena_destroy(&second_arena);
            ff_arena_destroy(&first_arena);
        }

        // ====================================================================
        // Save and load
        // ====================================================================
        static const size_t saved_header_size = 64;
        static const size_t saved_magic_offset = 0;
        static const size_t saved_version_offset = 4;
        static const size_t saved_block_size_offset = 8;
        static const size_t saved_hash_offset = 16;

        // The root block's own header, which is what carries the entry count and block size now.
        static const size_t block_count_offset = saved_header_size;
        static const size_t block_size_offset = saved_header_size + 4;
        static size_t block_values_offset(const ff_idict& dict)
        {
            return saved_header_size + block_header_size + count_of(&dict) * sizeof(uint64_t);
        }

        // Saves into a mutable heap copy so a test can corrupt one field and try to load it.
        static uint8_t* save_copy(const ff_idict& dict, ff_arena* arena, size_t* size)
        {
            ff_span saved = ff_idict_save(&dict, arena);
            uint8_t* bytes = (uint8_t*)malloc(saved.size);
            Assert::IsNotNull(bytes);
            memcpy(bytes, saved.data, saved.size);
            *size = saved.size;
            return bytes;
        }

        // Only ff_idict_verify looks at the content hash, so the corruption tests below leave it
        // stale on purpose: loading has to stand on its own structural checks.
        static void refresh_hash(uint8_t* bytes, size_t size)
        {
            uint64_t hash = ff_hash_bytes(bytes + saved_header_size, size - saved_header_size);
            memcpy(bytes + saved_hash_offset, &hash, sizeof(hash));
        }

        static bool try_load(const uint8_t* bytes, size_t size)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_idict loaded{};
            ff_span span{ bytes, size };
            bool loaded_ok = ff_idict_load(&loaded, &arena, span);

            ff_arena_destroy(&arena);
            return loaded_ok;
        }

        TEST_METHOD(save_and_load_round_trip)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            build_rich_source(&arena, &source);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            ff_span saved = ff_idict_save(&dict, &arena);
            Assert::AreEqual(saved_header_size + size_of(&dict), saved.size);

            ff_arena load_arena{};
            ff_arena_init_heap_global(&load_arena, 4096);

            ff_idict loaded{};
            Assert::IsTrue(ff_idict_load(&loaded, &load_arena, saved));

            Assert::AreEqual(count_of(&dict), count_of(&loaded));
            Assert::AreEqual(size_of(&dict), size_of(&loaded));
            Assert::IsTrue(loaded.data != dict.data);
            Assert::IsTrue(memcmp(loaded.data, dict.data, size_of(&dict)) == 0);
            verify_rich_block(loaded);

            // The loaded dict owns everything it needs, so the original can go away.
            ff_arena_destroy(&arena);
            verify_rich_block(loaded);

            ff_arena_destroy(&load_arena);
        }

        TEST_METHOD(saved_bytes_round_trip_through_a_file)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            build_rich_source(&arena, &source);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            ff_span saved = ff_idict_save(&dict, &arena);

            char folder[MAX_PATH];
            char path[MAX_PATH];
            Assert::IsTrue(GetTempPathA(MAX_PATH, folder) != 0);
            Assert::IsTrue(GetTempFileNameA(folder, "ffi", 0, path) != 0);

            FILE* file = nullptr;
            Assert::AreEqual(0, fopen_s(&file, path, "wb"));
            Assert::AreEqual(saved.size, fwrite(saved.data, 1, saved.size, file));
            Assert::AreEqual(0, fclose(file));

            Assert::AreEqual(0, fopen_s(&file, path, "rb"));
            Assert::AreEqual(0, fseek(file, 0, SEEK_END));
            long file_size = ftell(file);
            Assert::AreEqual((long)saved.size, file_size);
            rewind(file);

            uint8_t* bytes = (uint8_t*)malloc((size_t)file_size);
            Assert::IsNotNull(bytes);
            Assert::AreEqual((size_t)file_size, fread(bytes, 1, (size_t)file_size, file));
            Assert::AreEqual(0, fclose(file));
            Assert::IsTrue(DeleteFileA(path) != 0);

            ff_arena load_arena{};
            ff_arena_init_heap_global(&load_arena, 4096);

            ff_idict loaded{};
            ff_span span{ bytes, (size_t)file_size };
            Assert::IsTrue(ff_idict_load(&loaded, &load_arena, span));
            verify_rich_block(loaded);

            // The bytes read off disk are not the storage the dict runs from.
            free(bytes);
            verify_rich_block(loaded);

            ff_arena_destroy(&load_arena);
            ff_arena_destroy(&arena);
        }

        TEST_METHOD(load_puts_the_block_on_the_alignment_it_needs)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            build_rich_source(&arena, &source);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            size_t size = 0;
            uint8_t* bytes = save_copy(dict, &arena, &size);

            ff_arena load_arena{};
            ff_arena_init_heap_global(&load_arena, 4096);

            // Dirty the arena first so the block cannot come back aligned by luck.
            ff_arena_alloc(&load_arena, 1, 1);

            ff_idict loaded{};
            ff_span span{ bytes, size };
            Assert::IsTrue(ff_idict_load(&loaded, &load_arena, span));
            Assert::IsTrue(is_aligned(loaded.data, ff_idict_max_align));
            verify_rich_block(loaded);

            free(bytes);
            ff_arena_destroy(&load_arena);
            ff_arena_destroy(&arena);
        }

        TEST_METHOD(saved_prefix_is_padded_so_the_block_is_aligned_in_the_file)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            build_rich_source(&arena, &source);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            ff_span saved = ff_idict_save(&dict, &arena);

            // Blocks always want ff_idict_max_align, and the prefix is exactly that long, so a file
            // mapped at a page boundary already has its block correctly aligned.
            Assert::AreEqual((size_t)ff_idict_max_align, saved_header_size);
            Assert::IsTrue(is_aligned(saved.data, ff_idict_max_align));
            Assert::AreEqual(saved_header_size + size_of(&dict), saved.size);
            Assert::IsTrue(memcmp((const uint8_t*)saved.data + saved_header_size, dict.data, size_of(&dict)) == 0);

            // Everything the format does not use yet is zeroed, so future fields start from a
            // known state rather than from whatever was on the stack.
            for (size_t i = 32; i < saved_header_size; i++)
            {
                Assert::AreEqual((uint8_t)0, ((const uint8_t*)saved.data)[i]);
            }

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(load_ignores_the_content_hash_and_verify_checks_it)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            build_rich_source(&arena, &source);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            size_t size = 0;
            uint8_t* bytes = save_copy(dict, &arena, &size);

            Assert::IsTrue(try_load(bytes, size));
            Assert::IsTrue(ff_idict_verify(ff_span{ bytes, size }));

            // A byte deep in the payload that no structural check would ever notice. Loading still
            // succeeds, because loading never reads payload bytes, which is the whole point: a big
            // asset file must not have to be walked end to end just to be opened.
            bytes[size - 1] = (uint8_t)(bytes[size - 1] ^ 0xFF);
            Assert::IsTrue(try_load(bytes, size));
            Assert::IsFalse(ff_idict_verify(ff_span{ bytes, size }));

            // Only an explicit verify notices, and it passes again once the hash matches the bytes.
            refresh_hash(bytes, size);
            Assert::IsTrue(ff_idict_verify(ff_span{ bytes, size }));

            // Corrupting the recorded hash alone is enough to fail verification.
            bytes[saved_hash_offset] = (uint8_t)(bytes[saved_hash_offset] ^ 0xFF);
            Assert::IsFalse(ff_idict_verify(ff_span{ bytes, size }));
            Assert::IsTrue(try_load(bytes, size));

            free(bytes);
            ff_arena_destroy(&arena);
        }

        TEST_METHOD(verify_rejects_junk_before_it_hashes_anything)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            build_rich_source(&arena, &source);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            size_t size = 0;
            uint8_t* bytes = save_copy(dict, &arena, &size);

            Assert::IsFalse(ff_idict_verify(ff_span{ nullptr, 0 }));
            Assert::IsFalse(ff_idict_verify(ff_span{ bytes, 0 }));
            Assert::IsFalse(ff_idict_verify(ff_span{ bytes, saved_header_size - 1 }));
            Assert::IsFalse(ff_idict_verify(ff_span{ bytes, size - 1 }));

            uint32_t bogus = 0;
            memcpy(bytes + saved_magic_offset, &bogus, sizeof(bogus));
            Assert::IsFalse(ff_idict_verify(ff_span{ bytes, size }));

            free(bytes);
            ff_arena_destroy(&arena);
        }

        TEST_METHOD(empty_dict_round_trips_through_save)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            ff_span saved = ff_idict_save(&dict, &arena);
            Assert::AreEqual(saved_header_size + block_header_size, saved.size);

            ff_idict loaded{};
            Assert::IsTrue(ff_idict_load(&loaded, &arena, saved));
            Assert::AreEqual((size_t)0, count_of(&loaded));
            Assert::AreEqual(block_header_size, size_of(&loaded));
            Assert::IsNull(ff_idict_get(&loaded, sv("anything")));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(load_rejects_a_bad_magic)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            build_rich_source(&arena, &source);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            size_t size = 0;
            uint8_t* bytes = save_copy(dict, &arena, &size);

            Assert::IsTrue(try_load(bytes, size));
            bytes[saved_magic_offset] = (uint8_t)(bytes[saved_magic_offset] ^ 0xFF);
            Assert::IsFalse(try_load(bytes, size));

            free(bytes);
            ff_arena_destroy(&arena);
        }

        TEST_METHOD(load_rejects_a_bad_version)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            build_rich_source(&arena, &source);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            size_t size = 0;
            uint8_t* bytes = save_copy(dict, &arena, &size);

            uint32_t version = 99;
            memcpy(bytes + saved_version_offset, &version, sizeof(version));
            Assert::IsFalse(try_load(bytes, size));

            free(bytes);
            ff_arena_destroy(&arena);
        }

        TEST_METHOD(load_rejects_every_truncation)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            build_rich_source(&arena, &source);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            size_t size = 0;
            uint8_t* bytes = save_copy(dict, &arena, &size);

            // Any short read has to fail rather than run off the end of the buffer.
            for (size_t shorter = 0; shorter < size; shorter++)
            {
                Assert::IsFalse(try_load(bytes, shorter));
            }

            Assert::IsTrue(try_load(bytes, size));

            free(bytes);
            ff_arena_destroy(&arena);
        }

        TEST_METHOD(load_rejects_an_out_of_range_offset)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value text = ff_value_new_string(sv("hello"));
            ff_dict_set(&source, sv("text"), &text);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            size_t size = 0;
            uint8_t* bytes = save_copy(dict, &arena, &size);

            // ff_array_slice puts 'offset' first, so this is the string's offset field.
            size_t value_offset = block_values_offset(dict);
            size_t bogus = (size_t)1 << 40;
            memcpy(bytes + value_offset, &bogus, sizeof(bogus));

            Assert::IsFalse(try_load(bytes, size));

            free(bytes);
            ff_arena_destroy(&arena);
        }

        TEST_METHOD(load_rejects_a_count_that_does_not_fit)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            build_rich_source(&arena, &source);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            size_t size = 0;
            uint8_t* bytes = save_copy(dict, &arena, &size);

            // The count now lives in the block's own header, where it has to agree with the size.
            uint32_t count = 1000;
            memcpy(bytes + block_count_offset, &count, sizeof(count));
            Assert::IsFalse(try_load(bytes, size));

            // A count that overflows when multiplied out must be caught too.
            count = UINT32_MAX;
            memcpy(bytes + block_count_offset, &count, sizeof(count));
            Assert::IsFalse(try_load(bytes, size));

            free(bytes);
            ff_arena_destroy(&arena);
        }

        TEST_METHOD(load_rejects_a_block_header_size_that_disagrees_with_the_file)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            build_rich_source(&arena, &source);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            size_t size = 0;
            uint8_t* bytes = save_copy(dict, &arena, &size);

            // The root block has to fill the file exactly: claiming less would leave trailing bytes
            // unaccounted for, claiming more would run past the end.
            uint32_t block_size = (uint32_t)size_of(&dict) - 8;
            memcpy(bytes + block_size_offset, &block_size, sizeof(block_size));
            Assert::IsFalse(try_load(bytes, size));

            block_size = (uint32_t)size_of(&dict) + 8;
            memcpy(bytes + block_size_offset, &block_size, sizeof(block_size));
            Assert::IsFalse(try_load(bytes, size));

            block_size = (uint32_t)size_of(&dict);
            memcpy(bytes + block_size_offset, &block_size, sizeof(block_size));
            Assert::IsTrue(try_load(bytes, size));

            free(bytes);
            ff_arena_destroy(&arena);
        }

        TEST_METHOD(load_rejects_a_block_size_larger_than_the_bytes)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            build_rich_source(&arena, &source);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            size_t size = 0;
            uint8_t* bytes = save_copy(dict, &arena, &size);

            uint64_t block_size = 0;
            memcpy(&block_size, bytes + saved_block_size_offset, sizeof(block_size));
            block_size++;
            memcpy(bytes + saved_block_size_offset, &block_size, sizeof(block_size));

            Assert::IsFalse(try_load(bytes, size));

            free(bytes);
            ff_arena_destroy(&arena);
        }

        TEST_METHOD(load_rejects_an_alignment_the_format_cannot_honour)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            uint32_t payload[4] = { 1, 2, 3, 4 };
            ff_array_span as{};
            as.data = payload;
            as.count = 4;
            as.item_size = sizeof(uint32_t);
            as.item_align = alignof(uint32_t);

            ff_value value = ff_value_new_data_array(as);
            ff_dict_set(&source, sv("data"), &value);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            size_t size = 0;
            uint8_t* bytes = save_copy(dict, &arena, &size);
            Assert::IsTrue(try_load(bytes, size));

            size_t value_offset = block_values_offset(dict);
            ff_ivalue stored{};
            memcpy(&stored, bytes + value_offset, sizeof(stored));
            Assert::IsTrue(ff_value_type_data == stored.type);

            // Blocks are only ever allocated at ff_idict_max_align, so an item asking for more than
            // that could never be honoured and has to be refused rather than silently misaligned.
            stored.data.item_align = ff_idict_max_align * 2;
            memcpy(bytes + value_offset, &stored, sizeof(stored));
            Assert::IsFalse(try_load(bytes, size));

            // Nonsense alignments are refused outright.
            stored.data.item_align = 3;
            memcpy(bytes + value_offset, &stored, sizeof(stored));
            Assert::IsFalse(try_load(bytes, size));

            stored.data.item_align = 0;
            memcpy(bytes + value_offset, &stored, sizeof(stored));
            Assert::IsFalse(try_load(bytes, size));

            free(bytes);
            ff_arena_destroy(&arena);
        }

        TEST_METHOD(load_rejects_an_unknown_value_type)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value number = ff_value_new_int32(1);
            ff_dict_set(&source, sv("number"), &number);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            size_t size = 0;
            uint8_t* bytes = save_copy(dict, &arena, &size);

            // 'type' sits after the union inside the ff_ivalue.
            size_t type_offset = block_values_offset(dict) + offsetof(ff_ivalue, type);
            uint32_t bogus = 999;
            memcpy(bytes + type_offset, &bogus, sizeof(bogus));

            Assert::IsFalse(try_load(bytes, size));

            free(bytes);
            ff_arena_destroy(&arena);
        }

        TEST_METHOD(load_rejects_a_nested_dict_that_escapes_its_parent)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict inner{};
            ff_dict_init(&inner, &arena);

            ff_value inner_text = ff_value_new_string(sv("payload"));
            ff_dict_set(&inner, sv("text"), &inner_text);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value nested = ff_value_new_dict(&inner);
            ff_dict_set(&source, sv("child"), &nested);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            ff_idict child_dict = ff_ivalue_as_dict(ff_idict_get(&dict, sv("child")), &dict);

            size_t size = 0;
            uint8_t* bytes = save_copy(dict, &arena, &size);
            Assert::IsTrue(try_load(bytes, size));

            // A nested block claiming to be bigger than the data section it lives in.
            size_t value_offset = block_values_offset(dict);
            ff_ivalue value{};
            memcpy(&value, bytes + value_offset, sizeof(value));
            Assert::IsTrue(ff_value_type_dict == value.type);

            size_t nested_offset = saved_header_size + size_of(&dict) - size_of(&child_dict);
            uint32_t nested_size = (uint32_t)size_of(&child_dict);
            Assert::AreEqual(nested_offset, saved_header_size + data_start_of(count_of(&dict)) + value.data.offset);
            Assert::AreEqual((size_t)0, nested_offset % ff_idict_max_align);

            uint32_t bogus_size = nested_size + 64;
            memcpy(bytes + nested_offset + 4, &bogus_size, sizeof(bogus_size));
            Assert::IsFalse(try_load(bytes, size));

            memcpy(bytes + nested_offset + 4, &nested_size, sizeof(nested_size));
            Assert::IsTrue(try_load(bytes, size));

            // A nested dict value carries nothing but an offset, so anything else set in it is a
            // sign the bytes were not produced by this format.
            value.data.count = 1;
            memcpy(bytes + value_offset, &value, sizeof(value));
            Assert::IsFalse(try_load(bytes, size));

            free(bytes);
            ff_arena_destroy(&arena);
        }

        TEST_METHOD(load_leaves_the_arena_untouched_when_it_fails)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 64 * 1024);

            ff_dict source{};
            build_rich_source(&arena, &source);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            size_t size = 0;
            uint8_t* bytes = save_copy(dict, &arena, &size);

            uint32_t version = 0;
            memcpy(bytes + saved_version_offset, &version, sizeof(version));

            ff_arena load_arena{};
            ff_arena_init_heap_global(&load_arena, 64 * 1024);

            ff_arena_marker marker = ff_arena_mark(&load_arena);

            ff_idict loaded{};
            ff_span span{ bytes, size };
            Assert::IsFalse(ff_idict_load(&loaded, &load_arena, span));

            // A rejected file costs the caller nothing, and leaves an empty dict behind.
            Assert::IsTrue(ff_arena_mark(&load_arena) == marker);
            Assert::AreEqual((size_t)0, count_of(&loaded));
            Assert::IsNull(loaded.data);

            // A body that fails deep in validation has to rewind too.
            memcpy(bytes + saved_version_offset, &version, sizeof(version));
            uint32_t good_version = 1;
            memcpy(bytes + saved_version_offset, &good_version, sizeof(good_version));

            size_t value_offset = block_values_offset(dict);
            size_t bogus = (size_t)1 << 40;
            memcpy(bytes + value_offset, &bogus, sizeof(bogus));

            Assert::IsFalse(ff_idict_load(&loaded, &load_arena, span));
            Assert::IsTrue(ff_arena_mark(&load_arena) == marker);

            free(bytes);
            ff_arena_destroy(&load_arena);
            ff_arena_destroy(&arena);
        }

        // Touches every byte the dict claims to own, so an offset that escaped validation shows up
        // as a bad read rather than passing silently.
        static void walk_block(const ff_idict& dict, size_t depth, size_t* checksum)
        {
            if (depth > 64)
            {
                return;
            }

            const ff_ivalue* values = values_of(dict);

            for (size_t i = 0; i < count_of(&dict); i++)
            {
                *checksum += (size_t)keys_of(dict)[i];
                walk_value(values + i, dict, depth, checksum);
            }
        }

        static void walk_value(const ff_ivalue* value, const ff_idict& parent, size_t depth, size_t* checksum)
        {
            if (depth > 64)
            {
                return;
            }

            switch (value->type)
            {
                case ff_value_type_string:
                    {
                        ff_string_view text = ff_ivalue_as_string(value, &parent);
                        for (size_t i = 0; i < text.count; i++)
                        {
                            *checksum += (uint8_t)text.data[i];
                        }
                    }
                    break;

                case ff_value_type_data:
                    {
                        ff_array_span data = ff_ivalue_as_data(value, &parent);
                        const uint8_t* bytes = (const uint8_t*)data.data;
                        for (size_t i = 0; i < data.count * data.item_size; i++)
                        {
                            *checksum += bytes[i];
                        }
                    }
                    break;

                case ff_value_type_array:
                    {
                        ff_ivalue_span items = ff_ivalue_as_array(value, &parent);
                        for (size_t i = 0; i < items.count; i++)
                        {
                            walk_value(items.data + i, parent, depth + 1, checksum);
                        }
                    }
                    break;

                case ff_value_type_dict:
                    {
                        ff_idict child = ff_ivalue_as_dict(value, &parent);
                        walk_block(child, depth + 1, checksum);
                    }
                    break;

                default:
                    *checksum += (size_t)value->type;
                    break;
            }
        }

        TEST_METHOD(load_survives_random_corruption)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            build_rich_source(&arena, &source);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            size_t size = 0;
            uint8_t* clean = save_copy(dict, &arena, &size);

            uint8_t* bytes = (uint8_t*)malloc(size);
            Assert::IsNotNull(bytes);

            size_t accepted = 0;
            size_t checksum = 0;
            uint64_t random = 0x9E3779B97F4A7C15ULL; // fixed seed, so a failure is reproducible

            for (int iteration = 0; iteration < 3000; iteration++)
            {
                memcpy(bytes, clean, size);

                random = random * 6364136223846793005ULL + 1442695040888963407ULL;
                size_t index = (size_t)((random >> 33) % size);
                random = random * 6364136223846793005ULL + 1442695040888963407ULL;
                bytes[index] = (uint8_t)(random >> 33);

                ff_arena load_arena{};
                ff_arena_init_heap_global(&load_arena, 4096);

                ff_idict loaded{};
                ff_span span{ bytes, size };

                if (ff_idict_load(&loaded, &load_arena, span))
                {
                    // Whatever it accepted has to be fully walkable without leaving the block.
                    accepted++;
                    walk_block(loaded, 0, &checksum);
                }

                ff_arena_destroy(&load_arena);
            }

            // A single flipped byte is often harmless (it lands in payload bytes), so plenty should
            // still load. If nothing loaded the test would not be proving anything.
            Assert::IsTrue(accepted > 100);

            free(bytes);
            free(clean);
            ff_arena_destroy(&arena);
        }

        // ====================================================================
        // Stress
        // ====================================================================
        TEST_METHOD(many_entries_survive_scratch_growth)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            const int total = 400;
            char key[64];
            char text[64];
            // Each value needs its own buffer: a string value points at the caller's bytes.
            char texts[total][64];

            for (int i = 0; i < total; i++)
            {
                sprintf_s(key, "key%d", i);
                sprintf_s(texts[i], "value number %d padded out to move the block", i);
                ff_value value = ff_value_new_string(sv(texts[i]));
                ff_dict_set(&source, sv(key), &value);
            }

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            Assert::AreEqual((size_t)total, count_of(&dict));

            for (int i = 0; i < total; i++)
            {
                sprintf_s(key, "key%d", i);
                sprintf_s(text, "value number %d padded out to move the block", i);

                const ff_ivalue* found = ff_idict_get(&dict, sv(key));
                Assert::IsNotNull(found);

                ff_string_view stored = ff_ivalue_as_string(found, &dict);
                Assert::AreEqual(strlen(text), stored.count);
                Assert::IsTrue(memcmp(stored.data, text, stored.count) == 0);
            }

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(wide_tree_of_nested_dicts_survives_scratch_growth)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            const int children = 50;
            ff_dict inner[children]{};

            ff_dict source{};
            ff_dict_init(&source, &arena);

            char key[64];
            char text[64];
            // Each value needs its own buffer: a string value points at the caller's bytes.
            char texts[children][64];

            for (int i = 0; i < children; i++)
            {
                ff_dict_init(&inner[i], &arena);

                sprintf_s(texts[i], "child text %d", i);
                ff_value child_text = ff_value_new_string(sv(texts[i]));
                ff_value child_number = ff_value_new_int32(i);
                ff_dict_set(&inner[i], sv("text"), &child_text);
                ff_dict_set(&inner[i], sv("number"), &child_number);

                sprintf_s(key, "child%d", i);
                ff_value child = ff_value_new_dict(&inner[i]);
                ff_dict_set(&source, sv(key), &child);
            }

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            for (int i = 0; i < children; i++)
            {
                sprintf_s(key, "child%d", i);
                sprintf_s(text, "child text %d", i);

                ff_idict child_dict = ff_ivalue_as_dict(ff_idict_get(&dict, sv(key)), &dict);
                ff_string_view stored = ff_ivalue_as_string(ff_idict_get(&child_dict, sv("text")), &child_dict);

                Assert::AreEqual((size_t)2, count_of(&child_dict));
                Assert::AreEqual(i, ff_idict_get(&child_dict, sv("number"))->i32);
                Assert::AreEqual(strlen(text), stored.count);
                Assert::IsTrue(memcmp(stored.data, text, stored.count) == 0);
            }

            ff_arena_destroy(&arena);
        }

        // ====================================================================
        // Ordering
        // ====================================================================
        TEST_METHOD(entries_are_stored_sorted_by_key_hash)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            char key[32];

            for (int i = 0; i < 40; i++)
            {
                sprintf_s(key, "key%d", i);
                ff_value value = ff_value_new_int32(i);
                ff_dict_add(&source, sv(key), &value);
            }

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);
            Assert::AreEqual((size_t)40, count_of(&dict));

            for (size_t i = 1; i < count_of(&dict); i++)
            {
                Assert::IsTrue(keys_of(dict)[i - 1] <= keys_of(dict)[i]);
            }

            // Sorting must not lose anything: every key still resolves to the value it was given.
            for (int i = 0; i < 40; i++)
            {
                sprintf_s(key, "key%d", i);
                Assert::AreEqual(i, ff_idict_get(&dict, sv(key))->i32);
            }

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(insertion_order_does_not_change_the_bytes)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 8192);

            const char* names[5] = { "alpha", "beta", "gamma", "delta", "epsilon" };
            const int order_a[5] = { 0, 1, 2, 3, 4 };
            const int order_b[5] = { 3, 0, 4, 2, 1 };

            ff_dict nested_a{};
            ff_dict nested_b{};
            ff_dict_init(&nested_a, &arena);
            ff_dict_init(&nested_b, &arena);

            ff_dict source_a{};
            ff_dict source_b{};
            ff_dict_init(&source_a, &arena);
            ff_dict_init(&source_b, &arena);

            for (int i = 0; i < 5; i++)
            {
                ff_value inner_a = ff_value_new_int32(order_a[i]);
                ff_value inner_b = ff_value_new_int32(order_b[i]);
                ff_dict_set(&nested_a, sv(names[order_a[i]]), &inner_a);
                ff_dict_set(&nested_b, sv(names[order_b[i]]), &inner_b);

                ff_value text_a = ff_value_new_string(sv(names[order_a[i]]));
                ff_value text_b = ff_value_new_string(sv(names[order_b[i]]));
                ff_dict_set(&source_a, sv(names[order_a[i]]), &text_a);
                ff_dict_set(&source_b, sv(names[order_b[i]]), &text_b);
            }

            ff_value child_a = ff_value_new_dict(&nested_a);
            ff_value child_b = ff_value_new_dict(&nested_b);
            ff_dict_set(&source_a, sv("child"), &child_a);
            ff_dict_set(&source_b, sv("child"), &child_b);

            ff_idict dict_a{};
            ff_idict dict_b{};
            ff_idict_init(&dict_a, &arena, &source_a);
            ff_idict_init(&dict_b, &arena, &source_b);

            // Entries are emitted in key order and nothing unspecified is carried into the block, so
            // two dicts holding the same keys and values are the same bytes and hash the same. That
            // is what makes it safe to deduplicate or compare saved blocks by hash.
            Assert::AreEqual(size_of(&dict_a), size_of(&dict_b));
            Assert::IsTrue(memcmp(dict_a.data, dict_b.data, size_of(&dict_a)) == 0);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(unused_union_bytes_are_not_carried_into_the_block)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            // Two values that mean the same thing but whose unused union bytes differ. C leaves
            // those bytes unspecified, so they must not reach the block or identical dicts would
            // hash differently depending on what the caller's stack happened to hold.
            ff_value clean = ff_value_new_int32(7);

            ff_value dirty{};
            memset(&dirty, 0xCD, sizeof(dirty));
            dirty = ff_value_new_string(sv("scribble"));
            dirty.type = ff_value_type_int32;
            dirty.i32 = 7;

            ff_dict source_clean{};
            ff_dict source_dirty{};
            ff_dict_init(&source_clean, &arena);
            ff_dict_init(&source_dirty, &arena);
            ff_dict_set(&source_clean, sv("n"), &clean);
            ff_dict_set(&source_dirty, sv("n"), &dirty);

            ff_idict dict_clean{};
            ff_idict dict_dirty{};
            ff_idict_init(&dict_clean, &arena, &source_clean);
            ff_idict_init(&dict_dirty, &arena, &source_dirty);

            Assert::AreEqual(size_of(&dict_clean), size_of(&dict_dirty));
            Assert::IsTrue(memcmp(dict_clean.data, dict_dirty.data, size_of(&dict_clean)) == 0);
            Assert::AreEqual(7, ff_idict_get(&dict_dirty, sv("n"))->i32);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(duplicate_keys_keep_the_order_they_were_added_in)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 8192);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            // Enough other keys that the duplicates are not simply left where they started.
            char key[32];

            for (int i = 0; i < 30; i++)
            {
                sprintf_s(key, "other%d", i);
                ff_value filler = ff_value_new_int32(-1);
                ff_dict_add(&source, sv(key), &filler);

                ff_value dup = ff_value_new_int32(i);
                ff_dict_add(&source, sv("dup"), &dup);
            }

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            const ff_ivalue* found = ff_idict_get(&dict, sv("dup"));

            for (int i = 0; i < 30; i++)
            {
                Assert::IsNotNull(found);
                Assert::AreEqual(i, found->i32);
                found = ff_idict_get_next(&dict, sv("dup"), found);
            }

            Assert::IsNull(found);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(lookup_finds_every_key_past_the_binary_search_threshold)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            // Straddles the point where lookup stops scanning and starts binary searching, so both
            // paths are covered and they are checked to agree.
            for (int count = 1; count <= 40; count++)
            {
                ff_arena round_arena{};
                ff_arena_init_heap_global(&round_arena, 4096);

                ff_dict source{};
                ff_dict_init(&source, &round_arena);

                char key[32];

                for (int i = 0; i < count; i++)
                {
                    sprintf_s(key, "k%d", i);
                    ff_value value = ff_value_new_int32(i);
                    ff_dict_add(&source, sv(key), &value);
                }

                ff_idict dict{};
                ff_idict_init(&dict, &round_arena, &source);

                for (int i = 0; i < count; i++)
                {
                    sprintf_s(key, "k%d", i);
                    const ff_ivalue* found = ff_idict_get(&dict, sv(key));
                    Assert::IsNotNull(found);
                    Assert::AreEqual(i, found->i32);
                }

                Assert::IsNull(ff_idict_get(&dict, sv("absent")));
                ff_arena_destroy(&round_arena);
            }

            ff_arena_destroy(&arena);
        }

        // ====================================================================
        // Limits
        // ====================================================================
        TEST_METHOD(nesting_up_to_the_depth_limit_is_allowed)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 65536);

            // The depth cap is private to idict.c, so this mirrors it. The root is depth 0, so a
            // chain of this many dicts is the deepest legal one.
            const int depth = 64;
            ff_dict chain[depth]{};

            for (int i = depth - 1; i >= 0; i--)
            {
                ff_dict_init(&chain[i], &arena);

                ff_value marker = ff_value_new_int32(i);
                ff_dict_set(&chain[i], sv("depth"), &marker);

                if (i + 1 < depth)
                {
                    ff_value child = ff_value_new_dict(&chain[i + 1]);
                    ff_dict_set(&chain[i], sv("child"), &child);
                }
            }

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &chain[0]);

            ff_idict current = dict;

            for (int i = 0; i < depth; i++)
            {
                Assert::AreEqual(i, ff_idict_get(&current, sv("depth"))->i32);

                const ff_ivalue* child = ff_idict_get(&current, sv("child"));

                if (i + 1 < depth)
                {
                    Assert::IsNotNull(child);
                    current = ff_ivalue_as_dict(child, &current);
                }
                else
                {
                    Assert::IsNull(child);
                }
            }

            ff_arena_destroy(&arena);
        }

        // ====================================================================
        // Empty payloads
        // ====================================================================
        TEST_METHOD(empty_payloads_stay_inside_the_block)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value text = ff_value_new_string(sv(""));
            ff_dict_set(&source, sv("text"), &text);

            ff_span empty_span{};
            ff_value blob = ff_value_new_data(empty_span);
            ff_dict_set(&source, sv("blob"), &blob);

            ff_value_span empty_items{};
            ff_value list = ff_value_new_array(empty_items);
            ff_dict_set(&source, sv("list"), &list);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            // Nothing was appended, so this block has no data section at all. Every pointer handed
            // out for it still has to land inside the allocation rather than past the end of it.
            const uint8_t* start = (const uint8_t*)dict.data;
            const uint8_t* end = start + size_of(&dict);

            Assert::AreEqual((size_t)0, data_size_of(dict));

            const uint8_t* text_data = (const uint8_t*)ff_ivalue_as_string(ff_idict_get(&dict, sv("text")), &dict).data;
            const uint8_t* blob_data = (const uint8_t*)ff_ivalue_as_data(ff_idict_get(&dict, sv("blob")), &dict).data;
            const uint8_t* list_data = (const uint8_t*)ff_ivalue_as_array(ff_idict_get(&dict, sv("list")), &dict).data;

            Assert::IsTrue(text_data >= start && text_data <= end);
            Assert::IsTrue(blob_data >= start && blob_data <= end);
            Assert::IsTrue(list_data >= start && list_data <= end);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(an_empty_payload_does_not_pad_out_the_next_one)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            // An empty payload has nothing to align, so it must not push what follows it up to the
            // next 64 byte boundary for the sake of alignment it does not need.
            ff_array_span empty{};
            empty.data = &source;
            empty.count = 0;
            empty.item_size = 1;
            empty.item_align = ff_idict_max_align;

            ff_value blob = ff_value_new_data_array(empty);
            ff_dict_set(&source, sv("a_empty"), &blob);

            uint8_t bytes[3] = { 1, 2, 3 };
            ff_array_span filled{};
            filled.data = bytes;
            filled.count = 3;
            filled.item_size = 1;
            filled.item_align = 1;

            ff_value real = ff_value_new_data_array(filled);
            ff_dict_set(&source, sv("b_real"), &real);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            Assert::AreEqual((size_t)3, data_size_of(dict));
            Assert::IsTrue(memcmp(ff_ivalue_as_data(ff_idict_get(&dict, sv("b_real")), &dict).data, bytes, sizeof(bytes)) == 0);

            ff_arena_destroy(&arena);
        }

        // ====================================================================
        // Allocation
        // ====================================================================
        TEST_METHOD(building_leaves_the_arena_holding_only_the_block)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 1024 * 1024);

            ff_dict source{};
            build_rich_source(&arena, &source);

            // Whatever the build needs for itself comes out of the caller's arena and goes straight
            // back, so all the caller is charged for is the block, allocated once at its exact size.
            ff_arena_marker before = ff_arena_mark(&arena);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            ff_arena_marker after = ff_arena_mark(&arena);
            size_t block_size = size_of(&dict);
            size_t used = (size_t)(after - before);

            Assert::IsTrue((const uint8_t*)dict.data >= before);
            Assert::AreEqual(block_size, (size_t)(after - (const uint8_t*)dict.data));

            // Only the padding needed to reach the block's alignment may be charged on top of it.
            Assert::IsTrue(used >= block_size && used < block_size + ff_idict_max_align);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(a_deep_build_returns_all_of_its_scratch)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 64 * 1024);

            const int depth = 32;
            ff_dict chain[depth]{};

            for (int i = depth - 1; i >= 0; i--)
            {
                ff_dict_init(&chain[i], &arena);

                for (int k = 0; k < 20; k++)
                {
                    char key[32];
                    sprintf_s(key, "key%d", k);
                    ff_value value = ff_value_new_int32(i * 100 + k);
                    ff_dict_add(&chain[i], sv(key), &value);
                }

                if (i + 1 < depth)
                {
                    ff_value child = ff_value_new_dict(&chain[i + 1]);
                    ff_dict_set(&chain[i], sv("child"), &child);
                }
            }

            ff_arena_marker before = ff_arena_mark(&arena);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &chain[0]);

            // Every level sorts its own keys in scratch taken from this arena. Nested levels are
            // live at the same time, so this is the case that would leak if any level kept its own.
            size_t used = (size_t)(ff_arena_mark(&arena) - before);
            Assert::IsTrue(used < size_of(&dict) + ff_idict_max_align);

            ff_idict current = dict;

            for (int i = 0; i < depth; i++)
            {
                Assert::AreEqual(i * 100 + 7, ff_idict_get(&current, sv("key7"))->i32);

                if (i + 1 < depth)
                {
                    current = ff_ivalue_as_dict(ff_idict_get(&current, sv("child")), &current);
                }
            }

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(the_build_only_holds_scratch_for_the_path_it_is_on)
        {
            // Siblings are built one after another, so only the current path's key-sort scratch
            // should ever be live at once. If a level kept its scratch instead of handing it back,
            // peak usage would grow with the whole tree rather than its depth, and the outer
            // rewinds at the end of the build would hide that. This measures the peak directly, by
            // reading back how far into a poisoned buffer the build ever reached.
            const int siblings = 256;
            const int per_child = 32;

            ff_arena build_source_arena{};
            ff_arena_init_heap_global(&build_source_arena, 1024 * 1024);

            ff_dict source{};
            ff_dict_init(&source, &build_source_arena);

            ff_dict* children = ff_arena_alloc_type(&build_source_arena, ff_dict, siblings);
            Assert::IsNotNull(children);

            for (int i = 0; i < siblings; i++)
            {
                children[i] = ff_dict{};
                ff_dict_init(&children[i], &build_source_arena);

                for (int k = 0; k < per_child; k++)
                {
                    char key[32];
                    sprintf_s(key, "key%d", k);
                    ff_value value = ff_value_new_int32(i * 1000 + k);
                    ff_dict_add(&children[i], sv(key), &value);
                }

                char name[32];
                sprintf_s(name, "child%d", i);
                ff_value child = ff_value_new_dict(&children[i]);
                ff_dict_add(&source, sv(name), &child);
            }

            // The buffer is big enough that the build never has to grow it, and is poisoned first so
            // that the highest byte the build touched can be read back out of it afterwards. Rewinding
            // only moves the arena's bump pointer, it does not scrub what was written, so the poison
            // survives exactly where nothing was ever allocated.
            const uint8_t poison = 0xcd;
            size_t buffer_size = 4 * 1024 * 1024;
            uint8_t* buffer = (uint8_t*)_aligned_malloc(buffer_size, ff_idict_max_align);
            Assert::IsNotNull(buffer);
            memset(buffer, poison, buffer_size);

            ff_arena arena{};
            ff_arena_init_external(&arena, buffer, buffer_size, 4096);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            Assert::IsTrue((const uint8_t*)dict.data >= buffer && (const uint8_t*)dict.data < buffer + buffer_size);
            Assert::AreEqual((size_t)siblings, count_of(&dict));
            Assert::IsNull((void*)arena.buffer->next);

            size_t high_water = buffer_size;
            while (high_water && buffer[high_water - 1] == poison)
            {
                high_water--;
            }

            // The block is written in full, so anything above it is scratch that was live at the same
            // time. One root order plus one child order is a few KB; holding every sibling's at once
            // would be about 64 KB. The bound sits between the two.
            size_t block_size = size_of(&dict);
            size_t scratch_peak = high_water - (size_t)((const uint8_t*)dict.data - buffer) - block_size;
            Assert::IsTrue(scratch_peak < 16 * 1024, L"the build held on to more scratch than the path it was on needs");

            for (int i = 0; i < siblings; i += 37)
            {
                char name[32];
                sprintf_s(name, "child%d", i);
                ff_idict child = ff_ivalue_as_dict(ff_idict_get(&dict, sv(name)), &dict);
                Assert::AreEqual(i * 1000 + 7, ff_idict_get(&child, sv("key7"))->i32);
            }

            ff_arena_destroy(&arena);
            _aligned_free(buffer);
            ff_arena_destroy(&build_source_arena);
        }

        // ====================================================================
        // Format pinning
        // ====================================================================
        TEST_METHOD(saved_prefix_records_the_layout_the_block_depends_on)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            build_rich_source(&arena, &source);

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            ff_span saved = ff_idict_save(&dict, &arena);
            const uint8_t* bytes = (const uint8_t*)saved.data;

            uint16_t ivalue_size = 0;
            uint16_t block_align = 0;
            uint16_t value_type_count = 0;
            memcpy(&ivalue_size, bytes + 24, sizeof(ivalue_size));
            memcpy(&block_align, bytes + 26, sizeof(block_align));
            memcpy(&value_type_count, bytes + 28, sizeof(value_type_count));

            Assert::AreEqual((uint16_t)sizeof(ff_ivalue), ivalue_size);
            Assert::AreEqual((uint16_t)ff_idict_max_align, block_align);
            Assert::AreEqual((uint16_t)(ff_value_type_array + 1), value_type_count);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(load_rejects_a_file_whose_layout_constants_differ)
        {
            // A build whose entry size, alignment or set of value types has drifted would read every
            // one of these files as something other than what was written, so each is rejected.
            const size_t offsets[3] = { 24, 26, 28 };

            for (size_t i = 0; i < 3; i++)
            {
                ff_arena arena{};
                ff_arena_init_heap_global(&arena, 4096);

                ff_dict source{};
                build_rich_source(&arena, &source);

                ff_idict dict{};
                ff_idict_init(&dict, &arena, &source);

                ff_span saved = ff_idict_save(&dict, &arena);
                uint8_t* bytes = (uint8_t*)malloc(saved.size);
                Assert::IsNotNull(bytes);
                memcpy(bytes, saved.data, saved.size);

                uint16_t original = 0;
                memcpy(&original, bytes + offsets[i], sizeof(original));
                uint16_t changed = (uint16_t)(original + 1);
                memcpy(bytes + offsets[i], &changed, sizeof(changed));

                ff_arena load_arena{};
                ff_arena_init_heap_global(&load_arena, 4096);

                ff_idict loaded{};
                ff_span span{ bytes, saved.size };
                Assert::IsFalse(ff_idict_load(&loaded, &load_arena, span));
                Assert::IsNull(loaded.data);

                free(bytes);
                ff_arena_destroy(&load_arena);
                ff_arena_destroy(&arena);
            }
        }

        TEST_METHOD(load_rejects_keys_that_are_out_of_order)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            for (int i = 0; i < 8; i++)
            {
                char key[32];
                sprintf_s(key, "key%d", i);
                ff_value value = ff_value_new_int32(i);
                ff_dict_add(&source, sv(key), &value);
            }

            ff_idict dict{};
            ff_idict_init(&dict, &arena, &source);

            ff_span saved = ff_idict_save(&dict, &arena);
            uint8_t* bytes = (uint8_t*)malloc(saved.size);
            Assert::IsNotNull(bytes);
            memcpy(bytes, saved.data, saved.size);

            // Swap the first two keys. Lookups binary search, so a block whose keys are not sorted
            // would quietly fail to find entries that are really there.
            uint8_t* keys = bytes + saved_header_size + block_header_size;
            uint64_t first = 0;
            uint64_t second = 0;
            memcpy(&first, keys, sizeof(first));
            memcpy(&second, keys + sizeof(uint64_t), sizeof(second));
            memcpy(keys, &second, sizeof(second));
            memcpy(keys + sizeof(uint64_t), &first, sizeof(first));

            ff_arena load_arena{};
            ff_arena_init_heap_global(&load_arena, 4096);

            ff_idict loaded{};
            ff_span span{ bytes, saved.size };
            Assert::IsFalse(ff_idict_load(&loaded, &load_arena, span));
            Assert::IsNull(loaded.data);

            free(bytes);
            ff_arena_destroy(&load_arena);
            ff_arena_destroy(&arena);
        }
    };
}
