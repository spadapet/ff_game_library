#include "pch.h"

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
                ff_dict_add(&dict, ff_sz_view(key), &value);
            }

            Assert::IsTrue(dict.keys == keys);
            Assert::IsTrue(values_of(dict) == values);
            Assert::AreEqual((size_t)4, dict.capacity);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(init_overwrites_stale_fields)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            // init must fully overwrite the struct, not merge with whatever was there.
            ff_dict dict;
            memset(&dict, 0xCD, sizeof(dict));
            ff_dict_init(&dict, &arena);

            Assert::AreEqual((size_t)0, dict.count);
            Assert::AreEqual((size_t)0, dict.capacity);
            Assert::IsNull(dict.keys);
            Assert::IsTrue(dict.arena == &arena);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(init_capacity_one_grows_to_eight)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init_capacity(&dict, &arena, 1);

            Assert::AreEqual((size_t)1, dict.capacity);

            ff_value first = ff_value_new_int32(1);
            ff_value second = ff_value_new_int32(2);
            ff_dict_add(&dict, FF_SVL("a"), &first);
            ff_dict_add(&dict, FF_SVL("b"), &second);

            // Growth is round_up_pow2(max(capacity * 2, 8)), so 1 jumps straight to 8.
            Assert::AreEqual((size_t)8, dict.capacity);
            Assert::AreEqual(1, ff_dict_get(&dict, FF_SVL("a"))->i32);
            Assert::AreEqual(2, ff_dict_get(&dict, FF_SVL("b"))->i32);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(init_capacity_odd_growth_moves_overlapping_values)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            // Values move from offset capacity to offset new_capacity within one block, and for
            // small growth steps the source and destination ranges overlap.
            ff_dict dict{};
            ff_dict_init_capacity(&dict, &arena, 3);

            char key[32];
            for (int i = 0; i < 4; i++)
            {
                sprintf_s(key, "key%d", i);
                ff_value value = ff_value_new_int32(i);
                ff_dict_add(&dict, ff_sz_view(key), &value);
            }

            Assert::AreEqual((size_t)8, dict.capacity);

            for (int i = 0; i < 4; i++)
            {
                sprintf_s(key, "key%d", i);
                ff_value* found = ff_dict_get(&dict, ff_sz_view(key));
                Assert::IsNotNull(found);
                Assert::AreEqual(i, found->i32);
            }

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
            Assert::IsNull(ff_dict_get(&copy, FF_SVL("anything")));

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
            ff_dict_set(&source, FF_SVL("one"), &one);
            ff_dict_set(&source, FF_SVL("two"), &two);

            ff_dict copy{};
            ff_dict_init_copy(&copy, &arena, &source);

            Assert::AreEqual((size_t)2, copy.count);
            Assert::IsTrue(values_of(copy) != values_of(source));
            Assert::AreEqual(1, ff_dict_get(&copy, FF_SVL("one"))->i32);
            Assert::AreEqual(2, ff_dict_get(&copy, FF_SVL("two"))->i32);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(init_copy_is_independent_of_source)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value original = ff_value_new_int32(10);
            ff_dict_set(&source, FF_SVL("key"), &original);

            ff_dict copy{};
            ff_dict_init_copy(&copy, &arena, &source);

            ff_value changed = ff_value_new_int32(20);
            ff_dict_set(&source, FF_SVL("key"), &changed);

            Assert::AreEqual(20, ff_dict_get(&source, FF_SVL("key"))->i32);
            Assert::AreEqual(10, ff_dict_get(&copy, FF_SVL("key"))->i32);

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
            ff_dict_add(&source, FF_SVL("dup"), &one);
            ff_dict_add(&source, FF_SVL("dup"), &two);

            ff_dict copy{};
            ff_dict_init_copy(&copy, &arena, &source);

            Assert::AreEqual((size_t)2, copy.count);
            Assert::AreEqual(2, this->count_matches(&copy, "dup"));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(init_copy_uses_the_target_arena)
        {
            ff_arena source_arena{};
            ff_arena_init_heap_global(&source_arena, 4096);

            ff_arena copy_arena{};
            ff_arena_init_heap_global(&copy_arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &source_arena);

            ff_value value = ff_value_new_int32(7);
            ff_dict_set(&source, FF_SVL("key"), &value);

            ff_dict copy{};
            ff_dict_init_copy(&copy, &copy_arena, &source);

            Assert::IsTrue(copy.arena == &copy_arena);
            Assert::AreEqual(7, ff_dict_get(&copy, FF_SVL("key"))->i32);

            // The copy must not point back into the source arena's storage.
            ff_arena_destroy(&source_arena);
            Assert::AreEqual((size_t)1, copy.count);
            Assert::AreEqual(7, ff_dict_get(&copy, FF_SVL("key"))->i32);

            ff_arena_destroy(&copy_arena);
        }

        TEST_METHOD(init_copy_capacity_matches_source_count)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init_capacity(&source, &arena, 64);

            char key[32];
            for (int i = 0; i < 3; i++)
            {
                sprintf_s(key, "key%d", i);
                ff_value value = ff_value_new_int32(i);
                ff_dict_add(&source, ff_sz_view(key), &value);
            }

            ff_dict copy{};
            ff_dict_init_copy(&copy, &arena, &source);

            // The copy is sized to the live entries, not to the source's spare capacity.
            Assert::AreEqual((size_t)3, copy.count);
            Assert::AreEqual((size_t)3, copy.capacity);

            for (int i = 0; i < 3; i++)
            {
                sprintf_s(key, "key%d", i);
                Assert::AreEqual(i, ff_dict_get(&copy, ff_sz_view(key))->i32);
            }

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(init_copy_skips_cleared_entries)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value a = ff_value_new_int32(1);
            ff_value b = ff_value_new_int32(2);
            ff_value c = ff_value_new_int32(3);
            ff_dict_set(&source, FF_SVL("a"), &a);
            ff_dict_set(&source, FF_SVL("b"), &b);
            ff_dict_set(&source, FF_SVL("c"), &c);
            Assert::IsTrue(ff_dict_clear(&source, FF_SVL("b")));

            ff_dict copy{};
            ff_dict_init_copy(&copy, &arena, &source);

            Assert::AreEqual((size_t)2, copy.count);
            Assert::AreEqual(1, ff_dict_get(&copy, FF_SVL("a"))->i32);
            Assert::IsNull(ff_dict_get(&copy, FF_SVL("b")));
            Assert::AreEqual(3, ff_dict_get(&copy, FF_SVL("c"))->i32);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(init_copy_of_reset_dict_allocates_nothing)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value value = ff_value_new_int32(1);
            ff_dict_set(&source, FF_SVL("key"), &value);
            ff_dict_reset(&source);

            // The source still owns capacity, but with no live entries there is nothing to copy.
            ff_arena_marker marker = ff_arena_mark(&arena);

            ff_dict copy{};
            ff_dict_init_copy(&copy, &arena, &source);

            Assert::AreEqual((size_t)0, copy.count);
            Assert::AreEqual((size_t)0, copy.capacity);
            Assert::IsNull(copy.keys);
            Assert::IsTrue(ff_arena_mark(&arena) == marker);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(init_copy_result_is_a_usable_dict)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict source{};
            ff_dict_init(&source, &arena);

            ff_value value = ff_value_new_int32(1);
            ff_dict_set(&source, FF_SVL("key"), &value);

            ff_dict copy{};
            ff_dict_init_copy(&copy, &arena, &source);

            // The copy starts out full, so the next add has to grow it.
            char key[32];
            for (int i = 0; i < 10; i++)
            {
                sprintf_s(key, "extra%d", i);
                ff_value extra = ff_value_new_int32(100 + i);
                ff_dict_set(&copy, ff_sz_view(key), &extra);
            }

            Assert::AreEqual((size_t)11, copy.count);
            Assert::AreEqual(1, ff_dict_get(&copy, FF_SVL("key"))->i32);
            Assert::AreEqual(109, ff_dict_get(&copy, FF_SVL("extra9"))->i32);
            Assert::AreEqual((size_t)1, source.count);

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
            ff_dict_add(&dict, FF_SVL("answer"), &value);

            ff_value* found = ff_dict_get(&dict, FF_SVL("answer"));

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
            ff_dict_add(&dict, FF_SVL("present"), &value);

            Assert::IsNull(ff_dict_get(&dict, FF_SVL("missing")));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(get_from_empty_dict_returns_null)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            Assert::IsNull(ff_dict_get(&dict, FF_SVL("missing")));

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
            ff_dict_add(&dict, FF_SVL("dup"), &one);
            ff_dict_add(&dict, FF_SVL("dup"), &two);
            ff_dict_add(&dict, FF_SVL("dup"), &three);

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
            ff_dict_add(&dict, FF_SVL("dup"), &one);
            ff_dict_add(&dict, FF_SVL("dup"), &two);

            Assert::AreEqual(1, ff_dict_get(&dict, FF_SVL("dup"))->i32);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(add_grows_from_zero_capacity)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            ff_value value = ff_value_new_int32(1);
            ff_dict_add(&dict, FF_SVL("key"), &value);

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
                ff_dict_add(&dict, ff_sz_view(key), &value);
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
                ff_dict_add(&dict, ff_sz_view(key), &value);
            }

            Assert::AreEqual((size_t)40, dict.count);

            for (int i = 0; i < 40; i++)
            {
                sprintf_s(key, "key%d", i);
                ff_value* found = ff_dict_get(&dict, ff_sz_view(key));
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
                ff_dict_add(&dict, ff_sz_view(key), &value);
            }

            Assert::AreEqual((size_t)6, dict.count);
            Assert::AreEqual((size_t)16, dict.capacity);
            Assert::AreEqual(0, ff_dict_get(&dict, FF_SVL("key0"))->i32);
            Assert::AreEqual(5, ff_dict_get(&dict, FF_SVL("key5"))->i32);

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
            ff_dict_add(&dict, FF_SVL("key"), &value);

            ff_value* first = ff_dict_get(&dict, FF_SVL("key"));
            Assert::IsNotNull(first);
            Assert::IsTrue(first == values_of(dict));
            Assert::IsNull(ff_dict_get_next(&dict, FF_SVL("key"), first));

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
            ff_dict_add(&dict, FF_SVL("dup"), &one);
            ff_dict_add(&dict, FF_SVL("dup"), &two);
            ff_dict_add(&dict, FF_SVL("dup"), &four);

            int visited = 0;
            int sum = 0;

            for (ff_value* value = ff_dict_get(&dict, FF_SVL("dup")); value && visited < 16; value = ff_dict_get_next(&dict, FF_SVL("dup"), value))
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
            ff_dict_add(&dict, FF_SVL("dup"), &one);
            ff_dict_add(&dict, FF_SVL("dup"), &two);

            Assert::IsTrue(ff_dict_get_next(&dict, FF_SVL("dup"), nullptr) == ff_dict_get(&dict, FF_SVL("dup")));

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
            ff_dict_add(&dict, FF_SVL("dup"), &one);
            ff_dict_add(&dict, FF_SVL("other"), &other);
            ff_dict_add(&dict, FF_SVL("dup"), &two);

            // Oldest first, so the walk starts at 1 and steps forward past "other" to reach 2.
            ff_value* first = ff_dict_get(&dict, FF_SVL("dup"));
            Assert::AreEqual(1, first->i32);

            ff_value* second = ff_dict_get_next(&dict, FF_SVL("dup"), first);

            Assert::IsNotNull(second);
            Assert::AreEqual(2, second->i32);
            Assert::IsNull(ff_dict_get_next(&dict, FF_SVL("dup"), second));

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
            ff_dict_add(&dict, FF_SVL("a"), &first);
            ff_dict_add(&dict, FF_SVL("b"), &last);

            Assert::IsNull(ff_dict_get_next(&dict, FF_SVL("b"), values_of(dict) + 1));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(get_next_resumes_after_the_given_slot_of_any_key)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            ff_value first = ff_value_new_int32(1);
            ff_value middle = ff_value_new_int32(2);
            ff_value last = ff_value_new_int32(3);
            ff_dict_add(&dict, FF_SVL("target"), &first);
            ff_dict_add(&dict, FF_SVL("other"), &middle);
            ff_dict_add(&dict, FF_SVL("target"), &last);

            // prev_value only marks a position, so passing another key's entry is still valid.
            ff_value* found = ff_dict_get_next(&dict, FF_SVL("target"), values_of(dict) + 1);

            Assert::IsNotNull(found);
            Assert::AreEqual(3, found->i32);
            Assert::IsTrue(found == values_of(dict) + 2);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(get_next_on_empty_dict_returns_null)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            Assert::IsNull(ff_dict_get_next(&dict, FF_SVL("key"), nullptr));

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
                ff_dict_add(&dict, FF_SVL("dup"), &value);
            }

            int expected = 0;
            int visited = 0;

            for (ff_value* value = ff_dict_get(&dict, FF_SVL("dup")); value && visited < 64; value = ff_dict_get_next(&dict, FF_SVL("dup"), value))
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
            ff_dict_set(&dict, FF_SVL("key"), &value);

            Assert::AreEqual((size_t)1, dict.count);
            Assert::AreEqual(7, ff_dict_get(&dict, FF_SVL("key"))->i32);

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
            ff_dict_set(&dict, FF_SVL("key"), &first);
            ff_dict_set(&dict, FF_SVL("key"), &second);

            Assert::AreEqual((size_t)1, dict.count);
            Assert::AreEqual(2, ff_dict_get(&dict, FF_SVL("key"))->i32);

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
            ff_dict_add(&dict, FF_SVL("dup"), &one);
            ff_dict_add(&dict, FF_SVL("dup"), &two);
            ff_dict_set(&dict, FF_SVL("dup"), &final_value);

            Assert::AreEqual((size_t)1, dict.count);
            Assert::AreEqual(1, this->count_matches(&dict, "dup"));
            Assert::AreEqual(3, ff_dict_get(&dict, FF_SVL("dup"))->i32);

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
            ff_dict_set(&dict, FF_SVL("a"), &a);
            ff_dict_set(&dict, FF_SVL("b"), &b);
            ff_dict_set(&dict, FF_SVL("c"), &c);
            ff_dict_set(&dict, FF_SVL("b"), &b2);

            Assert::AreEqual((size_t)3, dict.count);
            Assert::AreEqual(1, ff_dict_get(&dict, FF_SVL("a"))->i32);
            Assert::AreEqual(20, ff_dict_get(&dict, FF_SVL("b"))->i32);
            Assert::AreEqual(3, ff_dict_get(&dict, FF_SVL("c"))->i32);

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
            ff_dict_set(&dict, FF_SVL("a"), &a);
            ff_dict_set(&dict, FF_SVL("b"), &b);
            ff_dict_set(&dict, FF_SVL("a"), &a2);

            Assert::AreEqual((size_t)2, dict.count);
            Assert::AreEqual(2, values_of(dict)[0].i32);
            Assert::AreEqual(10, values_of(dict)[1].i32);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(set_existing_key_at_full_capacity_does_not_grow)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init_capacity(&dict, &arena, 4);

            char key[32];
            for (int i = 0; i < 4; i++)
            {
                sprintf_s(key, "key%d", i);
                ff_value value = ff_value_new_int32(i);
                ff_dict_add(&dict, ff_sz_view(key), &value);
            }

            const uint64_t* keys = dict.keys;

            // Replacing removes before it adds, so a full dict has room again.
            ff_value replacement = ff_value_new_int32(99);
            ff_dict_set(&dict, FF_SVL("key0"), &replacement);

            Assert::AreEqual((size_t)4, dict.count);
            Assert::AreEqual((size_t)4, dict.capacity);
            Assert::IsTrue(dict.keys == keys);
            Assert::AreEqual(99, ff_dict_get(&dict, FF_SVL("key0"))->i32);
            Assert::AreEqual(3, ff_dict_get(&dict, FF_SVL("key3"))->i32);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(set_new_key_at_full_capacity_grows)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init_capacity(&dict, &arena, 4);

            char key[32];
            for (int i = 0; i < 4; i++)
            {
                sprintf_s(key, "key%d", i);
                ff_value value = ff_value_new_int32(i);
                ff_dict_add(&dict, ff_sz_view(key), &value);
            }

            ff_value extra = ff_value_new_int32(4);
            ff_dict_set(&dict, FF_SVL("key4"), &extra);

            Assert::AreEqual((size_t)5, dict.count);
            Assert::AreEqual((size_t)8, dict.capacity);

            for (int i = 0; i < 5; i++)
            {
                sprintf_s(key, "key%d", i);
                Assert::AreEqual(i, ff_dict_get(&dict, ff_sz_view(key))->i32);
            }

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
            ff_dict_set(&dict, FF_SVL("key"), &value);

            Assert::IsTrue(ff_dict_clear(&dict, FF_SVL("key")));
            Assert::AreEqual((size_t)0, dict.count);
            Assert::IsNull(ff_dict_get(&dict, FF_SVL("key")));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(clear_missing_key_returns_false)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            ff_value value = ff_value_new_int32(1);
            ff_dict_set(&dict, FF_SVL("key"), &value);

            Assert::IsFalse(ff_dict_clear(&dict, FF_SVL("other")));
            Assert::AreEqual((size_t)1, dict.count);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(clear_on_empty_dict_returns_false)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            Assert::IsFalse(ff_dict_clear(&dict, FF_SVL("key")));
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
                ff_dict_add(&dict, FF_SVL("dup"), &value);
            }

            Assert::IsTrue(ff_dict_clear(&dict, FF_SVL("dup")));
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
            ff_dict_add(&dict, FF_SVL("a"), &a);
            ff_dict_add(&dict, FF_SVL("dup"), &dup1);
            ff_dict_add(&dict, FF_SVL("b"), &b);
            ff_dict_add(&dict, FF_SVL("dup"), &dup2);
            ff_dict_add(&dict, FF_SVL("c"), &c);

            Assert::IsTrue(ff_dict_clear(&dict, FF_SVL("dup")));

            Assert::AreEqual((size_t)3, dict.count);
            Assert::AreEqual(1, values_of(dict)[0].i32);
            Assert::AreEqual(2, values_of(dict)[1].i32);
            Assert::AreEqual(3, values_of(dict)[2].i32);
            Assert::AreEqual(1, ff_dict_get(&dict, FF_SVL("a"))->i32);
            Assert::AreEqual(2, ff_dict_get(&dict, FF_SVL("b"))->i32);
            Assert::AreEqual(3, ff_dict_get(&dict, FF_SVL("c"))->i32);

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
            ff_dict_add(&dict, FF_SVL("a"), &a);
            ff_dict_add(&dict, FF_SVL("b"), &b);
            ff_dict_add(&dict, FF_SVL("c"), &c);

            Assert::IsTrue(ff_dict_clear(&dict, FF_SVL("a")));

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
            ff_dict_add(&dict, FF_SVL("a"), &a);
            ff_dict_add(&dict, FF_SVL("b"), &b);

            Assert::IsTrue(ff_dict_clear(&dict, FF_SVL("b")));

            Assert::AreEqual((size_t)1, dict.count);
            Assert::AreEqual(1, values_of(dict)[0].i32);
            Assert::IsNull(ff_dict_get(&dict, FF_SVL("b")));

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
                ff_dict_add(&dict, ff_sz_view(key), &value);
            }

            for (int i = 0; i < 10; i++)
            {
                sprintf_s(key, "key%d", i);
                Assert::IsTrue(ff_dict_clear(&dict, ff_sz_view(key)));
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
            ff_dict_add(&dict, FF_SVL("key"), &first);
            size_t capacity = dict.capacity;

            ff_dict_clear(&dict, FF_SVL("key"));

            ff_value second = ff_value_new_int32(2);
            ff_dict_add(&dict, FF_SVL("key"), &second);

            Assert::AreEqual((size_t)1, dict.count);
            Assert::AreEqual(capacity, dict.capacity);
            Assert::AreEqual(2, ff_dict_get(&dict, FF_SVL("key"))->i32);

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
                ff_dict_add(&dict, ff_sz_view(key), &value);
            }

            const ff_value* values = values_of(dict);
            size_t capacity = dict.capacity;

            ff_dict_reset(&dict);

            Assert::AreEqual((size_t)0, dict.count);
            Assert::AreEqual(capacity, dict.capacity);
            Assert::IsTrue(values_of(dict) == values);
            Assert::IsNull(ff_dict_get(&dict, FF_SVL("key0")));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(reset_then_add_works)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            ff_value first = ff_value_new_int32(1);
            ff_dict_add(&dict, FF_SVL("old"), &first);

            ff_dict_reset(&dict);

            ff_value second = ff_value_new_int32(2);
            ff_dict_add(&dict, FF_SVL("new"), &second);

            Assert::AreEqual((size_t)1, dict.count);
            Assert::IsNull(ff_dict_get(&dict, FF_SVL("old")));
            Assert::AreEqual(2, ff_dict_get(&dict, FF_SVL("new"))->i32);

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

            ff_dict_set(&dict, FF_SVL("null"), &null_value);
            ff_dict_set(&dict, FF_SVL("bool"), &bool_value);
            ff_dict_set(&dict, FF_SVL("int64"), &int64_value);
            ff_dict_set(&dict, FF_SVL("float64"), &float64_value);
            ff_dict_set(&dict, FF_SVL("point"), &point_value);
            ff_dict_set(&dict, FF_SVL("rect"), &rect_value);

            Assert::IsTrue(ff_value_type_null == ff_dict_get(&dict, FF_SVL("null"))->type);
            Assert::IsTrue(ff_dict_get(&dict, FF_SVL("bool"))->b);
            Assert::AreEqual((int64_t)1234567890123LL, ff_dict_get(&dict, FF_SVL("int64"))->i64);
            Assert::AreEqual(2.5, ff_dict_get(&dict, FF_SVL("float64"))->f64);
            Assert::AreEqual(3, ff_dict_get(&dict, FF_SVL("point"))->point_i32[0]);
            Assert::AreEqual(4, ff_dict_get(&dict, FF_SVL("point"))->point_i32[1]);
            Assert::AreEqual(4.0f, ff_dict_get(&dict, FF_SVL("rect"))->rect_f32[3]);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(stores_string_value)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            ff_value value = ff_value_new_string(FF_SVL("hello"));
            ff_dict_set(&dict, FF_SVL("greeting"), &value);

            ff_string_view text = ff_value_as_string(ff_dict_get(&dict, FF_SVL("greeting")));

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
            ff_dict_set(&inner, FF_SVL("inner_key"), &inner_value);

            ff_dict outer{};
            ff_dict_init(&outer, &arena);

            ff_value nested = ff_value_new_dict(&inner);
            ff_dict_set(&outer, FF_SVL("child"), &nested);

            ff_dict* found = ff_value_as_dict(ff_dict_get(&outer, FF_SVL("child")));

            Assert::IsTrue(found == &inner);
            Assert::AreEqual(99, ff_dict_get(found, FF_SVL("inner_key"))->i32);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(stores_data_value)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            uint8_t bytes[4] = { 1, 2, 3, 4 };
            ff_span span{ bytes, sizeof(bytes) };
            ff_value value = ff_value_new_data(span);
            ff_dict_set(&dict, FF_SVL("blob"), &value);

            ff_span found = ff_value_as_data(ff_dict_get(&dict, FF_SVL("blob")));

            Assert::AreEqual(sizeof(bytes), found.size);
            Assert::IsTrue(found.data == bytes);
            Assert::IsTrue(memcmp(found.data, bytes, sizeof(bytes)) == 0);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(stores_array_value)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            ff_value items[2] = { ff_value_new_int32(10), ff_value_new_int32(20) };
            ff_value_span items_span{ items, 2 };
            ff_value value = ff_value_new_array(items_span);
            ff_dict_set(&dict, FF_SVL("list"), &value);

            ff_value_span found = ff_value_as_array(ff_dict_get(&dict, FF_SVL("list")));

            Assert::AreEqual((size_t)2, found.count);
            Assert::AreEqual(10, found.data[0].i32);
            Assert::AreEqual(20, found.data[1].i32);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(stores_guid_value)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            GUID guid;
            memset(&guid, 0xAB, sizeof(guid));
            ff_value value = ff_value_new_guid(guid);
            ff_dict_set(&dict, FF_SVL("id"), &value);

            ff_value* found = ff_dict_get(&dict, FF_SVL("id"));

            // A GUID fills the whole payload, so the type must survive the copy into the dict.
            Assert::IsTrue(ff_value_type_guid == found->type);
            Assert::IsTrue(memcmp(&found->guid, &guid, sizeof(guid)) == 0);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(returned_pointer_allows_in_place_edit)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            ff_value value = ff_value_new_int32(1);
            ff_dict_set(&dict, FF_SVL("key"), &value);

            ff_dict_get(&dict, FF_SVL("key"))->i32 = 5;

            Assert::AreEqual(5, ff_dict_get(&dict, FF_SVL("key"))->i32);

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
                ff_dict_set(&dict, ff_sz_view(key), &value);
            }

            Assert::AreEqual((size_t)total, dict.count);

            for (int i = 0; i < total; i++)
            {
                sprintf_s(key, "key%d", i);
                ff_value* found = ff_dict_get(&dict, ff_sz_view(key));
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
                ff_dict_set(&dict, ff_sz_view(key), &value);
            }

            for (int i = 0; i < 100; i += 2)
            {
                sprintf_s(key, "key%d", i);
                Assert::IsTrue(ff_dict_clear(&dict, ff_sz_view(key)));
            }

            Assert::AreEqual((size_t)50, dict.count);

            for (int i = 0; i < 100; i++)
            {
                sprintf_s(key, "key%d", i);
                ff_value* found = ff_dict_get(&dict, ff_sz_view(key));

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
            ff_dict_set(&dict, FF_SVL(""), &value);

            Assert::IsNotNull(ff_dict_get(&dict, FF_SVL("")));
            Assert::AreEqual(1, ff_dict_get(&dict, FF_SVL(""))->i32);
            Assert::IsNull(ff_dict_get(&dict, FF_SVL("other")));

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
            ff_dict_set(&dict, FF_SVL("key"), &lower);
            ff_dict_set(&dict, FF_SVL("KEY"), &upper);

            Assert::AreEqual((size_t)2, dict.count);
            Assert::AreEqual(1, ff_dict_get(&dict, FF_SVL("key"))->i32);
            Assert::AreEqual(2, ff_dict_get(&dict, FF_SVL("KEY"))->i32);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(keys_are_counted_not_null_terminated)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            const char text[] = "abc";
            ff_value shorter = ff_value_new_int32(1);
            ff_value longer = ff_value_new_int32(2);
            ff_dict_set(&dict, ff_string_view{ text, 2 }, &shorter);
            ff_dict_set(&dict, ff_string_view{ text, 3 }, &longer);

            Assert::AreEqual((size_t)2, dict.count);
            Assert::AreEqual(1, ff_dict_get(&dict, ff_string_view{ text, 2 })->i32);
            Assert::AreEqual(2, ff_dict_get(&dict, ff_string_view{ text, 3 })->i32);
            Assert::IsNull(ff_dict_get(&dict, ff_string_view{ text, 1 }));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(keys_may_contain_null_bytes)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            const char embedded[] = "a\0b";
            ff_value with_null = ff_value_new_int32(1);
            ff_value without_null = ff_value_new_int32(2);
            ff_dict_set(&dict, ff_string_view{ embedded, 3 }, &with_null);
            ff_dict_set(&dict, ff_string_view{ embedded, 1 }, &without_null);

            Assert::AreEqual((size_t)2, dict.count);
            Assert::AreEqual(1, ff_dict_get(&dict, ff_string_view{ embedded, 3 })->i32);
            Assert::AreEqual(2, ff_dict_get(&dict, ff_string_view{ embedded, 1 })->i32);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(key_lookup_does_not_depend_on_the_key_buffer)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            char key[32];
            sprintf_s(key, "temporary");
            ff_value value = ff_value_new_int32(5);
            ff_dict_set(&dict, ff_sz_view(key), &value);

            // Only the hash is stored, so overwriting the caller's buffer changes nothing.
            memset(key, 0, sizeof(key));

            Assert::AreEqual(5, ff_dict_get(&dict, FF_SVL("temporary"))->i32);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(set_after_reset_reuses_the_dict)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            ff_dict_init(&dict, &arena);

            ff_value first = ff_value_new_int32(1);
            ff_dict_set(&dict, FF_SVL("key"), &first);
            ff_dict_reset(&dict);

            ff_value second = ff_value_new_int32(2);
            ff_dict_set(&dict, FF_SVL("key"), &second);

            Assert::AreEqual((size_t)1, dict.count);
            Assert::AreEqual(1, this->count_matches(&dict, "key"));
            Assert::AreEqual(2, ff_dict_get(&dict, FF_SVL("key"))->i32);

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
                ff_dict_add(&dict, ff_sz_view(key), &value);
            }

            const uint64_t* keys = dict.keys;
            Assert::AreEqual((size_t)8, dict.capacity);

            ff_value extra = ff_value_new_int32(8);
            ff_dict_add(&dict, FF_SVL("key8"), &extra);

            Assert::AreEqual((size_t)16, dict.capacity);
            Assert::IsTrue(dict.keys == keys);

            for (int i = 0; i < 9; i++)
            {
                sprintf_s(key, "key%d", i);
                ff_value* found = ff_dict_get(&dict, ff_sz_view(key));
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
                ff_dict_add(&dict, ff_sz_view(key), &value);
            }

            const uint64_t* keys = dict.keys;
            Assert::IsNotNull(ff_arena_alloc(&arena, 64, 8));

            ff_value extra = ff_value_new_int32(8);
            ff_dict_add(&dict, FF_SVL("key8"), &extra);

            Assert::AreEqual((size_t)16, dict.capacity);
            Assert::IsTrue(dict.keys != keys);

            for (int i = 0; i < 9; i++)
            {
                sprintf_s(key, "key%d", i);
                ff_value* found = ff_dict_get(&dict, ff_sz_view(key));
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
                ff_dict_add(&dict, ff_sz_view(key), &value);

                for (int j = 0; j <= i; j++)
                {
                    sprintf_s(key, "key%d", j);
                    ff_value* found = ff_dict_get(&dict, ff_sz_view(key));
                    Assert::IsNotNull(found);
                    Assert::AreEqual((int64_t)j * 1000000007LL, found->i64);
                }
            }

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(two_dicts_growing_in_one_arena_stay_independent)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict first{};
            ff_dict_init(&first, &arena);

            ff_dict second{};
            ff_dict_init(&second, &arena);

            // Interleaved growth means neither dict is reliably the last arena allocation,
            // so both have to survive being relocated out from under each other.
            char key[32];
            for (int i = 0; i < 100; i++)
            {
                sprintf_s(key, "key%d", i);

                ff_value first_value = ff_value_new_int32(i);
                ff_dict_add(&first, ff_sz_view(key), &first_value);

                ff_value second_value = ff_value_new_int32(-i);
                ff_dict_add(&second, ff_sz_view(key), &second_value);
            }

            Assert::AreEqual((size_t)100, first.count);
            Assert::AreEqual((size_t)100, second.count);
            Assert::IsTrue(first.keys != second.keys);

            for (int i = 0; i < 100; i++)
            {
                sprintf_s(key, "key%d", i);
                Assert::AreEqual(i, ff_dict_get(&first, ff_sz_view(key))->i32);
                Assert::AreEqual(-i, ff_dict_get(&second, ff_sz_view(key))->i32);
            }

            ff_arena_destroy(&arena);
        }

    private:
        int count_matches(const ff_dict* dict, const char* key)
        {
            int visited = 0;

            for (ff_value* value = ff_dict_get(dict, ff_sz_view(key)); value && visited < 1024; value = ff_dict_get_next(dict, ff_sz_view(key), value))
            {
                visited++;
            }

            return visited;
        }
    };
}
