#include "pch.h"

namespace base_test
{
    template<bool AllowDupes>
    using test_hash_set = ff::internal::hash_set<std::string, ff::hash<std::string>, std::equal_to<std::string>, AllowDupes>;

    template<bool AllowDupes>
    static void empty_test()
    {
        test_hash_set<AllowDupes> set;
        Assert::IsTrue(set.empty());
        Assert::AreEqual<size_t>(0, set.size());
    }

    template<bool AllowDupes>
    static void insert_value_test()
    {
        test_hash_set<AllowDupes> set;
        std::string str1 = "copied";
        std::string str2 = "moved";

        auto i = set.insert(str1);
        Assert::AreEqual<std::string>("copied", *i.first);
        Assert::IsTrue(i.second);

        i = set.insert(str1);
        Assert::AreEqual<std::string>("copied", *i.first);
        Assert::AreEqual(AllowDupes, i.second);

        i = set.emplace("emplaced1");
        Assert::AreEqual<std::string>("emplaced1", *i.first);
        Assert::IsTrue(i.second);

        i = set.emplace("emplaced1");
        Assert::AreEqual<std::string>("emplaced1", *i.first);
        Assert::AreEqual(AllowDupes, i.second);

        i = set.emplace("emplaced2");
        Assert::AreEqual<std::string>("emplaced2", *i.first);
        Assert::IsTrue(i.second);

        i = set.emplace("emplaced2");
        Assert::AreEqual<std::string>("emplaced2", *i.first);
        Assert::AreEqual(AllowDupes, i.second);

        i = set.insert(std::move(str2));
        Assert::AreEqual<std::string>("moved", *i.first);
        Assert::IsTrue(i.second);

        i = set.insert(std::string("moved"));
        Assert::AreEqual<std::string>("moved", *i.first);
        Assert::AreEqual(AllowDupes, i.second);

        Assert::IsFalse(set.empty());
        Assert::AreEqual<size_t>(4 * (AllowDupes ? 2 : 1), set.size());

        set.erase(str1);
        Assert::AreEqual<size_t>(3 * (AllowDupes ? 2 : 1), set.size());

        set.erase(set.cbegin(), set.cend());
        Assert::IsTrue(set.empty());
        Assert::AreEqual<size_t>(0, set.size());
    }

    TEST_CLASS(hash_set_test)
    {
    public:
        TEST_METHOD(empty)
        {
            empty_test<true>();
            empty_test<false>();
        }

        TEST_METHOD(insert_value)
        {
            insert_value_test<true>();
            insert_value_test<false>();
        }
    };
}
