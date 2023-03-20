#include "pch.h"

static std::atomic_int next_value{1};

namespace
{
    struct test_obj
    {
        int value;

        test_obj() : value(::next_value.fetch_add(1)) {}
        auto operator<=>(const test_obj&) const = default;
    };
}

namespace ff::test::base
{
    TEST_CLASS(stash_tests)
    {
    public:
        TEST_METHOD(stashing)
        {
            ff::stash<::test_obj> stash;
            ::test_obj* a = stash.get_obj();
            ::test_obj* b = stash.get_obj();

            ::test_obj a0 = *a;
            ::test_obj b0 = *b;

            stash.stash_obj(a);
            a = stash.get_obj();
            Assert::AreEqual(a->value, a0.value);

            stash.stash_obj(b);
            b = stash.get_obj();
            Assert::AreEqual(b->value, b0.value);

            stash.stash_obj(a);
            stash.stash_obj(b);
        }

        TEST_METHOD(moving)
        {
            ff::stash<::test_obj> stash1;
            ::test_obj* a = stash1.get_obj();
            ::test_obj* b = stash1.get_obj();
            ::test_obj a0 = *a;
            ::test_obj b0 = *b;

            ff::stash<::test_obj> stash2 = std::move(stash1);
            stash2.stash_obj(a);
            stash2.stash_obj(b);

            stash1 = std::move(stash2); // reverses linked list of stashed objects
            a = stash1.get_obj();
            b = stash1.get_obj();
            Assert::AreEqual(a->value, a0.value);
            Assert::AreEqual(b->value, b0.value);
            stash1.stash_obj(a);
            stash1.stash_obj(b);
        }
    };
}
