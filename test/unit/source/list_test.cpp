#include "pch.h"

namespace base_test
{
    TEST_CLASS(list_test)
    {
    public:
        TEST_METHOD(empty_list)
        {
            ff::list<int> list;
            Assert::IsTrue(list.empty());
            Assert::IsTrue(list.cbegin() == list.cend());

            list.clear();
            Assert::IsTrue(list.empty());
        }

        TEST_METHOD(push_front)
        {
            ff::list<int> list1;
            list1.push_front(3);
            list1.push_front(2);
            list1.push_front(1);

            ff::list<int> list2{ 1, 2, 3 };
            Assert::IsTrue(list1 == list2);

            Assert::AreEqual(1, list1.front());
            Assert::AreEqual(1, list2.front());

            auto iter = list1.cbegin();
            Assert::AreEqual(1, *iter++);
            Assert::AreEqual(2, *iter++);
            Assert::AreEqual(3, *iter++);
            Assert::IsTrue(list1.cend() == iter);

            *list1.begin() = 4;
            Assert::AreEqual(4, *list1.cbegin());

            list1.clear();
            Assert::IsTrue(list1.empty());

            list2.clear();
            Assert::IsTrue(list2.empty());
        }

        TEST_METHOD(resize)
        {
            ff::list<int> list{ 1, 2, 3 };

            list.resize(1);
            Assert::IsTrue(list == ff::list<int>{ 1 });

            list.resize(4, 8);
            Assert::IsTrue(list == ff::list<int>{ 1, 8, 8, 8 });

            list.resize(0);
            Assert::IsTrue(list.empty());
        }

        TEST_METHOD(reverse_and_remove)
        {
            ff::list<int> list{ 1, 1, 2, 2, 3, 4 };

            list.reverse();
            Assert::IsTrue(list == ff::list<int>{ 4, 3, 2, 2, 1, 1 });

            Assert::AreEqual<size_t>(2, list.remove(2));
            Assert::IsTrue(list == ff::list<int>{ 4, 3, 1, 1 });

            Assert::AreEqual<size_t>(2, list.remove_if([](int i) { return i == 1; }));
            Assert::IsTrue(list == ff::list<int>{ 4, 3 });

            Assert::AreEqual<size_t>(1, list.remove_if([](int i) { return i == 4; }));
            Assert::IsTrue(list == ff::list<int>{ 3 });

            Assert::AreEqual<size_t>(1, list.remove_if([](int i) { return i == 3; }));
            Assert::IsTrue(list.empty());

            list.reverse();
            Assert::IsTrue(list.empty());
        }

        TEST_METHOD(swap)
        {
            ff::list<int> list1{ 1, 2, 3 };
            ff::list<int> list2{ 2, 4 };

            std::swap(list1, list2);
            Assert::IsTrue(list1 == ff::list<int>{ 2, 4 });
            Assert::IsTrue(list2 == ff::list<int>{ 1, 2, 3 });

            std::swap(list1, list2);
            Assert::IsTrue(list1 == ff::list<int>{ 1, 2, 3 });
            Assert::IsTrue(list2 == ff::list<int>{ 2, 4 });

            std::swap(list1, ff::list<int>());
            Assert::IsTrue(list1.empty());
        }

        TEST_METHOD(splice)
        {
            this->splice_test<false>();
            this->splice_test<true>();
        }

    private:
        template<bool SharedNodePool>
        void splice_test()
        {
            ff::list<int, SharedNodePool> list{ 1, 2, 3, 4, 5, 6, 7, 8 };

            list.splice(list.cbegin(), list);
            Assert::IsTrue(list == ff::list<int, SharedNodePool>{ 1, 2, 3, 4, 5, 6, 7, 8 });

            list.splice(list.cbegin(), list, std::next(list.cbegin(), 3));
            Assert::IsTrue(list == ff::list<int, SharedNodePool>{ 4, 5, 6, 7, 8, 1, 2, 3 });

            ff::list<int, SharedNodePool> list2;
            list2.splice(list2.cbegin(), list, list.cbegin(), std::next(list.cbegin(), 4));
            Assert::IsTrue(list == ff::list<int, SharedNodePool>{ 8, 1, 2, 3 });
            Assert::IsTrue(list2 == ff::list<int, SharedNodePool>{ 4, 5, 6, 7 });

            list2.splice(list2.cbegin(), list);
            Assert::IsTrue(list == ff::list<int, SharedNodePool>{});
            Assert::IsTrue(list2 == ff::list<int, SharedNodePool>{ 8, 1, 2, 3, 4, 5, 6, 7 });
        }
    };
}
