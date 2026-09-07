#include "pch.h"

static ff_string_view sv(const char* text)
{
    ff_string_view result{ text, strlen(text) };
    return result;
}

// The dict derives its values pointer instead of storing it, so tests derive it the same way.
static ff_value* values_of(const ff_dict& dict)
{
    return (ff_value*)(dict.keys + dict.capacity);
}

namespace ff::test::base
{
    TEST_CLASS(dict_tests)
    {
    public:
        // ====================================================================
        // Initialization
        // ====================================================================
        TEST_METHOD(init_creates_empty_dict)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            Assert::AreEqual((size_t)0, dict.count);
            Assert::AreEqual((size_t)0, dict.capacity);
            Assert::IsNull(dict.keys);
            Assert::IsTrue(dict.arena == &arena);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(init_capacity_reserves_storage)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init_capacity(&dict, &arena, 16);

            Assert::AreEqual((size_t)0, dict.count);
            Assert::AreEqual((size_t)16, dict.capacity);
            Assert::IsNotNull(dict.keys);
            Assert::IsNotNull(values_of(dict));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(init_capacity_zero_allocates_nothing)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_arena_marker marker = ff_arena_mark(&arena);

            ff_dict dict{};
            ff_dict_init_capacity(&dict, &arena, 0);

            Assert::AreEqual((size_t)0, dict.capacity);
            Assert::IsNull(dict.keys);
            Assert::IsTrue(ff_arena_mark(&arena) == marker);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(init_capacity_does_not_relocate_until_exceeded)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init_capacity(&dict, &arena, 4);

            const uint64_t* keys = dict.keys;
            const ff_value* values = values_of(dict);

            char key[32];
            for (int i = 0; i < 4; i++)
            {
                sprintf_s(key, "key%d", i);
                ff_value value = ff_value_new_int32(i);
                ff_dict_add(&dict, sv(key), &value);
            }

            Assert::IsTrue(dict.keys == keys);
            Assert::IsTrue(values_of(dict) == values);
            Assert::AreEqual((size_t)4, dict.capacity);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(init_copy_of_empty_dict)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_dict copy{};
            ff_dict_init_copy(&copy, &arena, &source);

            Assert::AreEqual((size_t)0, copy.count);
            Assert::IsNull(ff_dict_get(&copy, sv("anything")));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(init_copy_duplicates_entries)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value one = ff_value_new_int32(1);
            ff_value two = ff_value_new_int32(2);
            ff_dict_set(&source, sv("one"), &one);
            ff_dict_set(&source, sv("two"), &two);

            ff_dict copy{};
            ff_dict_init_copy(&copy, &arena, &source);

            Assert::AreEqual((size_t)2, copy.count);
            Assert::IsTrue(values_of(copy) != values_of(source));
            Assert::AreEqual(1, ff_dict_get(&copy, sv("one"))->i32);
            Assert::AreEqual(2, ff_dict_get(&copy, sv("two"))->i32);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(init_copy_is_independent_of_source)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value original = ff_value_new_int32(10);
            ff_dict_set(&source, sv("key"), &original);

            ff_dict copy{};
            ff_dict_init_copy(&copy, &arena, &source);

            ff_value changed = ff_value_new_int32(20);
            ff_dict_set(&source, sv("key"), &changed);

            Assert::AreEqual(20, ff_dict_get(&source, sv("key"))->i32);
            Assert::AreEqual(10, ff_dict_get(&copy, sv("key"))->i32);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(init_copy_preserves_duplicate_keys)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value one = ff_value_new_int32(1);
            ff_value two = ff_value_new_int32(2);
            ff_dict_add(&source, sv("dup"), &one);
            ff_dict_add(&source, sv("dup"), &two);

            ff_dict copy{};
            ff_dict_init_copy(&copy, &arena, &source);

            Assert::AreEqual((size_t)2, copy.count);
            Assert::AreEqual(2, this->count_matches(&copy, "dup"));

            ff_arena_destroy(&arena);
        }
        // ====================================================================
        // Add and get
        // ====================================================================
        TEST_METHOD(add_then_get_returns_value)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            ff_value value = ff_value_new_int32(42);
            ff_dict_add(&dict, sv("answer"), &value);

            ff_value* found = ff_dict_get(&dict, sv("answer"));

            Assert::IsNotNull(found);
            Assert::IsTrue(ff_value_type_int32 == found->type);
            Assert::AreEqual(42, found->i32);
            Assert::AreEqual((size_t)1, dict.count);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(get_missing_key_returns_null)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            ff_value value = ff_value_new_int32(1);
            ff_dict_add(&dict, sv("present"), &value);

            Assert::IsNull(ff_dict_get(&dict, sv("missing")));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(get_from_empty_dict_returns_null)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            Assert::IsNull(ff_dict_get(&dict, sv("missing")));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(add_keeps_duplicate_keys)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            ff_value one = ff_value_new_int32(1);
            ff_value two = ff_value_new_int32(2);
            ff_value three = ff_value_new_int32(3);
            ff_dict_add(&dict, sv("dup"), &one);
            ff_dict_add(&dict, sv("dup"), &two);
            ff_dict_add(&dict, sv("dup"), &three);

            Assert::AreEqual((size_t)3, dict.count);
            Assert::AreEqual(3, this->count_matches(&dict, "dup"));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(get_returns_first_of_duplicates)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            ff_value one = ff_value_new_int32(1);
            ff_value two = ff_value_new_int32(2);
            ff_dict_add(&dict, sv("dup"), &one);
            ff_dict_add(&dict, sv("dup"), &two);

            Assert::AreEqual(1, ff_dict_get(&dict, sv("dup"))->i32);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(add_grows_from_zero_capacity)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            ff_value value = ff_value_new_int32(1);
            ff_dict_add(&dict, sv("key"), &value);

            Assert::AreEqual((size_t)1, dict.count);
            Assert::AreEqual((size_t)8, dict.capacity);
            Assert::IsNotNull(dict.keys);
            Assert::IsNotNull(values_of(dict));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(add_growth_uses_power_of_two_capacity)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            char key[32];
            for (int i = 0; i < 9; i++)
            {
                sprintf_s(key, "key%d", i);
                ff_value value = ff_value_new_int32(i);
                ff_dict_add(&dict, sv(key), &value);
            }

            Assert::AreEqual((size_t)9, dict.count);
            Assert::AreEqual((size_t)16, dict.capacity);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(add_growth_preserves_earlier_entries)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            char key[32];
            for (int i = 0; i < 40; i++)
            {
                sprintf_s(key, "key%d", i);
                ff_value value = ff_value_new_int32(i);
                ff_dict_add(&dict, sv(key), &value);
            }

            Assert::AreEqual((size_t)40, dict.count);

            for (int i = 0; i < 40; i++)
            {
                sprintf_s(key, "key%d", i);
                ff_value* found = ff_dict_get(&dict, sv(key));
                Assert::IsNotNull(found);
                Assert::AreEqual(i, found->i32);
            }

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(add_growth_from_nonzero_initial_capacity)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init_capacity(&dict, &arena, 5);

            char key[32];
            for (int i = 0; i < 6; i++)
            {
                sprintf_s(key, "key%d", i);
                ff_value value = ff_value_new_int32(i);
                ff_dict_add(&dict, sv(key), &value);
            }

            Assert::AreEqual((size_t)6, dict.count);
            Assert::AreEqual((size_t)16, dict.capacity);
            Assert::AreEqual(0, ff_dict_get(&dict, sv("key0"))->i32);
            Assert::AreEqual(5, ff_dict_get(&dict, sv("key5"))->i32);

            ff_arena_destroy(&arena);
        }
        // ====================================================================
        // get_next iteration
        // ====================================================================
        TEST_METHOD(get_next_after_only_entry_returns_null)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            ff_value value = ff_value_new_int32(1);
            ff_dict_add(&dict, sv("key"), &value);

            ff_value* first = ff_dict_get(&dict, sv("key"));
            Assert::IsNotNull(first);
            Assert::IsTrue(first == values_of(dict));
            Assert::IsNull(ff_dict_get_next(&dict, sv("key"), first));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(get_next_iterates_duplicates_starting_at_index_zero)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            ff_value one = ff_value_new_int32(1);
            ff_value two = ff_value_new_int32(2);
            ff_value four = ff_value_new_int32(4);
            ff_dict_add(&dict, sv("dup"), &one);
            ff_dict_add(&dict, sv("dup"), &two);
            ff_dict_add(&dict, sv("dup"), &four);

            int visited = 0;
            int sum = 0;

            for (ff_value* value = ff_dict_get(&dict, sv("dup")); value && visited < 16; value = ff_dict_get_next(&dict, sv("dup"), value))
            {
                visited++;
                sum += value->i32;
            }

            Assert::AreEqual(3, visited);
            Assert::AreEqual(7, sum);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(get_next_with_null_returns_first_match)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            ff_value one = ff_value_new_int32(1);
            ff_value two = ff_value_new_int32(2);
            ff_dict_add(&dict, sv("dup"), &one);
            ff_dict_add(&dict, sv("dup"), &two);

            Assert::IsTrue(ff_dict_get_next(&dict, sv("dup"), nullptr) == ff_dict_get(&dict, sv("dup")));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(get_next_skips_entries_with_other_keys)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            ff_value one = ff_value_new_int32(1);
            ff_value other = ff_value_new_int32(99);
            ff_value two = ff_value_new_int32(2);
            ff_dict_add(&dict, sv("dup"), &one);
            ff_dict_add(&dict, sv("other"), &other);
            ff_dict_add(&dict, sv("dup"), &two);

            ff_value* first = ff_dict_get(&dict, sv("dup"));
            ff_value* second = ff_dict_get_next(&dict, sv("dup"), first);

            Assert::IsNotNull(second);
            Assert::AreEqual(2, second->i32);
            Assert::IsNull(ff_dict_get_next(&dict, sv("dup"), second));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(get_next_from_last_entry_returns_null)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            ff_value first = ff_value_new_int32(1);
            ff_value last = ff_value_new_int32(2);
            ff_dict_add(&dict, sv("a"), &first);
            ff_dict_add(&dict, sv("b"), &last);

            Assert::IsNull(ff_dict_get_next(&dict, sv("b"), values_of(dict) + 1));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(get_next_on_empty_dict_returns_null)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            Assert::IsNull(ff_dict_get_next(&dict, sv("key"), nullptr));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(get_next_visits_every_duplicate_exactly_once)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            for (int i = 0; i < 20; i++)
            {
                ff_value value = ff_value_new_int32(i);
                ff_dict_add(&dict, sv("dup"), &value);
            }

            int expected = 0;
            int visited = 0;

            for (ff_value* value = ff_dict_get(&dict, sv("dup")); value && visited < 64; value = ff_dict_get_next(&dict, sv("dup"), value))
            {
                Assert::AreEqual(expected++, value->i32);
                visited++;
            }

            Assert::AreEqual(20, visited);

            ff_arena_destroy(&arena);
        }

        // ====================================================================
        // set
        // ====================================================================
        TEST_METHOD(set_adds_missing_key)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            ff_value value = ff_value_new_int32(7);
            ff_dict_set(&dict, sv("key"), &value);

            Assert::AreEqual((size_t)1, dict.count);
            Assert::AreEqual(7, ff_dict_get(&dict, sv("key"))->i32);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(set_replaces_existing_value)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            ff_value first = ff_value_new_int32(1);
            ff_value second = ff_value_new_int32(2);
            ff_dict_set(&dict, sv("key"), &first);
            ff_dict_set(&dict, sv("key"), &second);

            Assert::AreEqual((size_t)1, dict.count);
            Assert::AreEqual(2, ff_dict_get(&dict, sv("key"))->i32);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(set_collapses_duplicates_to_single_entry)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            ff_value one = ff_value_new_int32(1);
            ff_value two = ff_value_new_int32(2);
            ff_value final_value = ff_value_new_int32(3);
            ff_dict_add(&dict, sv("dup"), &one);
            ff_dict_add(&dict, sv("dup"), &two);
            ff_dict_set(&dict, sv("dup"), &final_value);

            Assert::AreEqual((size_t)1, dict.count);
            Assert::AreEqual(1, this->count_matches(&dict, "dup"));
            Assert::AreEqual(3, ff_dict_get(&dict, sv("dup"))->i32);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(set_preserves_other_keys)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            ff_value a = ff_value_new_int32(1);
            ff_value b = ff_value_new_int32(2);
            ff_value c = ff_value_new_int32(3);
            ff_value b2 = ff_value_new_int32(20);
            ff_dict_set(&dict, sv("a"), &a);
            ff_dict_set(&dict, sv("b"), &b);
            ff_dict_set(&dict, sv("c"), &c);
            ff_dict_set(&dict, sv("b"), &b2);

            Assert::AreEqual((size_t)3, dict.count);
            Assert::AreEqual(1, ff_dict_get(&dict, sv("a"))->i32);
            Assert::AreEqual(20, ff_dict_get(&dict, sv("b"))->i32);
            Assert::AreEqual(3, ff_dict_get(&dict, sv("c"))->i32);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(set_moves_replaced_entry_to_end)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            ff_value a = ff_value_new_int32(1);
            ff_value b = ff_value_new_int32(2);
            ff_value a2 = ff_value_new_int32(10);
            ff_dict_set(&dict, sv("a"), &a);
            ff_dict_set(&dict, sv("b"), &b);
            ff_dict_set(&dict, sv("a"), &a2);

            Assert::AreEqual((size_t)2, dict.count);
            Assert::AreEqual(2, values_of(dict)[0].i32);
            Assert::AreEqual(10, values_of(dict)[1].i32);

            ff_arena_destroy(&arena);
        }
        // ====================================================================
        // clear
        // ====================================================================
        TEST_METHOD(clear_existing_key_returns_true)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            ff_value value = ff_value_new_int32(1);
            ff_dict_set(&dict, sv("key"), &value);

            Assert::IsTrue(ff_dict_clear(&dict, sv("key")));
            Assert::AreEqual((size_t)0, dict.count);
            Assert::IsNull(ff_dict_get(&dict, sv("key")));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(clear_missing_key_returns_false)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            ff_value value = ff_value_new_int32(1);
            ff_dict_set(&dict, sv("key"), &value);

            Assert::IsFalse(ff_dict_clear(&dict, sv("other")));
            Assert::AreEqual((size_t)1, dict.count);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(clear_on_empty_dict_returns_false)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            Assert::IsFalse(ff_dict_clear(&dict, sv("key")));
            Assert::AreEqual((size_t)0, dict.count);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(clear_removes_every_duplicate)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            for (int i = 0; i < 5; i++)
            {
                ff_value value = ff_value_new_int32(i);
                ff_dict_add(&dict, sv("dup"), &value);
            }

            Assert::IsTrue(ff_dict_clear(&dict, sv("dup")));
            Assert::AreEqual((size_t)0, dict.count);
            Assert::AreEqual(0, this->count_matches(&dict, "dup"));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(clear_preserves_order_of_remaining_entries)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            ff_value a = ff_value_new_int32(1);
            ff_value dup1 = ff_value_new_int32(100);
            ff_value b = ff_value_new_int32(2);
            ff_value dup2 = ff_value_new_int32(200);
            ff_value c = ff_value_new_int32(3);
            ff_dict_add(&dict, sv("a"), &a);
            ff_dict_add(&dict, sv("dup"), &dup1);
            ff_dict_add(&dict, sv("b"), &b);
            ff_dict_add(&dict, sv("dup"), &dup2);
            ff_dict_add(&dict, sv("c"), &c);

            Assert::IsTrue(ff_dict_clear(&dict, sv("dup")));

            Assert::AreEqual((size_t)3, dict.count);
            Assert::AreEqual(1, values_of(dict)[0].i32);
            Assert::AreEqual(2, values_of(dict)[1].i32);
            Assert::AreEqual(3, values_of(dict)[2].i32);
            Assert::AreEqual(1, ff_dict_get(&dict, sv("a"))->i32);
            Assert::AreEqual(2, ff_dict_get(&dict, sv("b"))->i32);
            Assert::AreEqual(3, ff_dict_get(&dict, sv("c"))->i32);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(clear_first_entry_shifts_the_rest_down)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            ff_value a = ff_value_new_int32(1);
            ff_value b = ff_value_new_int32(2);
            ff_value c = ff_value_new_int32(3);
            ff_dict_add(&dict, sv("a"), &a);
            ff_dict_add(&dict, sv("b"), &b);
            ff_dict_add(&dict, sv("c"), &c);

            Assert::IsTrue(ff_dict_clear(&dict, sv("a")));

            Assert::AreEqual((size_t)2, dict.count);
            Assert::AreEqual(2, values_of(dict)[0].i32);
            Assert::AreEqual(3, values_of(dict)[1].i32);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(clear_last_entry_keeps_the_rest)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            ff_value a = ff_value_new_int32(1);
            ff_value b = ff_value_new_int32(2);
            ff_dict_add(&dict, sv("a"), &a);
            ff_dict_add(&dict, sv("b"), &b);

            Assert::IsTrue(ff_dict_clear(&dict, sv("b")));

            Assert::AreEqual((size_t)1, dict.count);
            Assert::AreEqual(1, values_of(dict)[0].i32);
            Assert::IsNull(ff_dict_get(&dict, sv("b")));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(clear_all_keys_leaves_empty_dict)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            char key[32];
            for (int i = 0; i < 10; i++)
            {
                sprintf_s(key, "key%d", i);
                ff_value value = ff_value_new_int32(i);
                ff_dict_add(&dict, sv(key), &value);
            }

            for (int i = 0; i < 10; i++)
            {
                sprintf_s(key, "key%d", i);
                Assert::IsTrue(ff_dict_clear(&dict, sv(key)));
            }

            Assert::AreEqual((size_t)0, dict.count);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(clear_then_add_reuses_capacity)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            ff_value first = ff_value_new_int32(1);
            ff_dict_add(&dict, sv("key"), &first);
            size_t capacity = dict.capacity;

            ff_dict_clear(&dict, sv("key"));

            ff_value second = ff_value_new_int32(2);
            ff_dict_add(&dict, sv("key"), &second);

            Assert::AreEqual((size_t)1, dict.count);
            Assert::AreEqual(capacity, dict.capacity);
            Assert::AreEqual(2, ff_dict_get(&dict, sv("key"))->i32);

            ff_arena_destroy(&arena);
        }

        // ====================================================================
        // reset
        // ====================================================================
        TEST_METHOD(reset_empties_dict_but_keeps_capacity)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            char key[32];
            for (int i = 0; i < 5; i++)
            {
                sprintf_s(key, "key%d", i);
                ff_value value = ff_value_new_int32(i);
                ff_dict_add(&dict, sv(key), &value);
            }

            const ff_value* values = values_of(dict);
            size_t capacity = dict.capacity;

            ff_dict_reset(&dict);

            Assert::AreEqual((size_t)0, dict.count);
            Assert::AreEqual(capacity, dict.capacity);
            Assert::IsTrue(values_of(dict) == values);
            Assert::IsNull(ff_dict_get(&dict, sv("key0")));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(reset_then_add_works)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            ff_value first = ff_value_new_int32(1);
            ff_dict_add(&dict, sv("old"), &first);

            ff_dict_reset(&dict);

            ff_value second = ff_value_new_int32(2);
            ff_dict_add(&dict, sv("new"), &second);

            Assert::AreEqual((size_t)1, dict.count);
            Assert::IsNull(ff_dict_get(&dict, sv("old")));
            Assert::AreEqual(2, ff_dict_get(&dict, sv("new"))->i32);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(reset_on_empty_dict_is_safe)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            ff_dict_reset(&dict);

            Assert::AreEqual((size_t)0, dict.count);

            ff_arena_destroy(&arena);
        }
        // ====================================================================
        // Value round tripping
        // ====================================================================
        TEST_METHOD(stores_every_scalar_value_type)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            ff_value null_value = ff_value_new_null();
            ff_value bool_value = ff_value_new_boolean(true);
            ff_value int64_value = ff_value_new_int64(1234567890123LL);
            ff_value float64_value = ff_value_new_float64(2.5);
            ff_value point_value = ff_value_new_point_int32(3, 4);
            ff_value rect_value = ff_value_new_rect_float32(1.0f, 2.0f, 3.0f, 4.0f);

            ff_dict_set(&dict, sv("null"), &null_value);
            ff_dict_set(&dict, sv("bool"), &bool_value);
            ff_dict_set(&dict, sv("int64"), &int64_value);
            ff_dict_set(&dict, sv("float64"), &float64_value);
            ff_dict_set(&dict, sv("point"), &point_value);
            ff_dict_set(&dict, sv("rect"), &rect_value);

            Assert::IsTrue(ff_value_type_null == ff_dict_get(&dict, sv("null"))->type);
            Assert::IsTrue(ff_dict_get(&dict, sv("bool"))->b);
            Assert::AreEqual((int64_t)1234567890123LL, ff_dict_get(&dict, sv("int64"))->i64);
            Assert::AreEqual(2.5, ff_dict_get(&dict, sv("float64"))->f64);
            Assert::AreEqual(3, ff_dict_get(&dict, sv("point"))->point_i32[0]);
            Assert::AreEqual(4, ff_dict_get(&dict, sv("point"))->point_i32[1]);
            Assert::AreEqual(4.0f, ff_dict_get(&dict, sv("rect"))->rect_f32[3]);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(stores_string_value)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            ff_value value = ff_value_new_string(sv("hello"));
            ff_dict_set(&dict, sv("greeting"), &value);

            ff_string_view text = ff_value_as_string(ff_dict_get(&dict, sv("greeting")));

            Assert::AreEqual((size_t)5, text.count);
            Assert::IsTrue(memcmp(text.data, "hello", 5) == 0);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(stores_nested_dict_value)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict inner{};
            ff_dict_init(&inner, &arena);

            ff_value inner_value = ff_value_new_int32(99);
            ff_dict_set(&inner, sv("inner_key"), &inner_value);

            ff_dict outer{};
            ff_dict_init(&outer, &arena);

            ff_value nested = ff_value_new_dict(&inner);
            ff_dict_set(&outer, sv("child"), &nested);

            ff_dict* found = ff_value_as_dict(ff_dict_get(&outer, sv("child")));

            Assert::IsTrue(found == &inner);
            Assert::AreEqual(99, ff_dict_get(found, sv("inner_key"))->i32);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(returned_pointer_allows_in_place_edit)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            ff_value value = ff_value_new_int32(1);
            ff_dict_set(&dict, sv("key"), &value);

            ff_dict_get(&dict, sv("key"))->i32 = 5;

            Assert::AreEqual(5, ff_dict_get(&dict, sv("key"))->i32);

            ff_arena_destroy(&arena);
        }

        // ====================================================================
        // Stress
        // ====================================================================
        TEST_METHOD(many_keys_all_retrievable_after_repeated_growth)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            const int total = 500;
            char key[32];

            for (int i = 0; i < total; i++)
            {
                sprintf_s(key, "key%d", i);
                ff_value value = ff_value_new_int32(i);
                ff_dict_set(&dict, sv(key), &value);
            }

            Assert::AreEqual((size_t)total, dict.count);

            for (int i = 0; i < total; i++)
            {
                sprintf_s(key, "key%d", i);
                ff_value* found = ff_dict_get(&dict, sv(key));
                Assert::IsNotNull(found);
                Assert::AreEqual(i, found->i32);
            }

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(interleaved_set_and_clear_stays_consistent)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            char key[32];

            for (int i = 0; i < 100; i++)
            {
                sprintf_s(key, "key%d", i);
                ff_value value = ff_value_new_int32(i);
                ff_dict_set(&dict, sv(key), &value);
            }

            for (int i = 0; i < 100; i += 2)
            {
                sprintf_s(key, "key%d", i);
                Assert::IsTrue(ff_dict_clear(&dict, sv(key)));
            }

            Assert::AreEqual((size_t)50, dict.count);

            for (int i = 0; i < 100; i++)
            {
                sprintf_s(key, "key%d", i);
                ff_value* found = ff_dict_get(&dict, sv(key));

                if (i % 2 == 0)
                {
                    Assert::IsNull(found);
                }
                else
                {
                    Assert::IsNotNull(found);
                    Assert::AreEqual(i, found->i32);
                }
            }

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(empty_key_is_a_valid_key)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            ff_value value = ff_value_new_int32(1);
            ff_dict_set(&dict, sv(""), &value);

            Assert::IsNotNull(ff_dict_get(&dict, sv("")));
            Assert::AreEqual(1, ff_dict_get(&dict, sv(""))->i32);
            Assert::IsNull(ff_dict_get(&dict, sv("other")));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(keys_are_case_sensitive)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            ff_value lower = ff_value_new_int32(1);
            ff_value upper = ff_value_new_int32(2);
            ff_dict_set(&dict, sv("key"), &lower);
            ff_dict_set(&dict, sv("KEY"), &upper);

            Assert::AreEqual((size_t)2, dict.count);
            Assert::AreEqual(1, ff_dict_get(&dict, sv("key"))->i32);
            Assert::AreEqual(2, ff_dict_get(&dict, sv("KEY"))->i32);

            ff_arena_destroy(&arena);
        }

        // ====================================================================
        // Storage layout
        // ====================================================================
        TEST_METHOD(keys_and_values_share_one_allocation)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_arena_marker marker = ff_arena_mark(&arena);

            ff_dict dict{};
            ff_dict_init_capacity(&dict, &arena, 16);

            // The 16 key slots are followed immediately by the 16 value slots, filling one block.
            Assert::IsTrue((const uint8_t*)dict.keys == marker);
            Assert::IsTrue((const uint8_t*)(values_of(dict) + 16) == ff_arena_mark(&arena));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(sizeof_dict_is_32_bytes)
        {
            Assert::AreEqual((size_t)32, sizeof(ff_dict));
        }

        TEST_METHOD(init_capacity_makes_exactly_one_allocation)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_arena_marker marker = ff_arena_mark(&arena);

            ff_dict dict{};
            ff_dict_init_capacity(&dict, &arena, 8);

            size_t used = (size_t)(ff_arena_mark(&arena) - marker);

            Assert::AreEqual(8 * (sizeof(uint64_t) + sizeof(ff_value)), used);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(growth_extends_in_place_when_dict_is_last_allocation)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 64 * 1024);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            char key[32];
            for (int i = 0; i < 8; i++)
            {
                sprintf_s(key, "key%d", i);
                ff_value value = ff_value_new_int32(i);
                ff_dict_add(&dict, sv(key), &value);
            }

            const uint64_t* keys = dict.keys;
            Assert::AreEqual((size_t)8, dict.capacity);

            ff_value extra = ff_value_new_int32(8);
            ff_dict_add(&dict, sv("key8"), &extra);

            Assert::AreEqual((size_t)16, dict.capacity);
            Assert::IsTrue(dict.keys == keys);

            for (int i = 0; i < 9; i++)
            {
                sprintf_s(key, "key%d", i);
                ff_value* found = ff_dict_get(&dict, sv(key));
                Assert::IsNotNull(found);
                Assert::AreEqual(i, found->i32);
            }

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(growth_relocates_when_dict_is_not_last_allocation)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 64 * 1024);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            char key[32];
            for (int i = 0; i < 8; i++)
            {
                sprintf_s(key, "key%d", i);
                ff_value value = ff_value_new_int32(i);
                ff_dict_add(&dict, sv(key), &value);
            }

            const uint64_t* keys = dict.keys;
            Assert::IsNotNull(ff_arena_alloc(&arena, 64, 8));

            ff_value extra = ff_value_new_int32(8);
            ff_dict_add(&dict, sv("key8"), &extra);

            Assert::AreEqual((size_t)16, dict.capacity);
            Assert::IsTrue(dict.keys != keys);

            for (int i = 0; i < 9; i++)
            {
                sprintf_s(key, "key%d", i);
                ff_value* found = ff_dict_get(&dict, sv(key));
                Assert::IsNotNull(found);
                Assert::AreEqual(i, found->i32);
            }

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(values_survive_repositioning_at_every_growth_step)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            char key[32];
            for (int i = 0; i < 300; i++)
            {
                sprintf_s(key, "key%d", i);
                ff_value value = ff_value_new_int64((int64_t)i * 1000000007LL);
                ff_dict_add(&dict, sv(key), &value);

                for (int j = 0; j <= i; j++)
                {
                    sprintf_s(key, "key%d", j);
                    ff_value* found = ff_dict_get(&dict, sv(key));
                    Assert::IsNotNull(found);
                    Assert::AreEqual((int64_t)j * 1000000007LL, found->i64);
                }
            }

            ff_arena_destroy(&arena);
        }

    private:
        int count_matches(const ff_dict* dict, const char* key)
        {
            int visited = 0;

            for (ff_value* value = ff_dict_get(dict, sv(key)); value && visited < 1024; value = ff_dict_get_next(dict, sv(key), value))
            {
                visited++;
            }

            return visited;
        }
    };
}