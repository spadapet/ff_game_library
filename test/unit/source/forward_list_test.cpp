#include "pch.h"

namespace base_test
{
    TEST_CLASS(forward_list_test)
    {
    public:
        TEST_METHOD(empty_forward_list)
        {
            ff::forward_list<int> list;
            Assert::IsTrue(list.empty());
            Assert::IsTrue(list.cbegin() == list.cend());

            list.clear();
            Assert::IsTrue(list.empty());
        }

        TEST_METHOD(push_front)
        {
            ff::forward_list<int> list1;
            list1.push_front(3);
            list1.push_front(2);
            list1.push_front(1);

            ff::forward_list<int> list2{ 1, 2, 3 };
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
            ff::forward_list<int> list{ 1, 2, 3 };

            list.resize(1);
            Assert::IsTrue(list == ff::forward_list<int>{ 1 });

            list.resize(4, 8);
            Assert::IsTrue(list == ff::forward_list<int>{ 1, 8, 8, 8 });

            list.resize(0);
            Assert::IsTrue(list.empty());
        }

        TEST_METHOD(reverse_and_remove)
        {
            ff::forward_list<int> list{ 1, 1, 2, 2, 3, 4 };

            list.reverse();
            Assert::IsTrue(list == ff::forward_list<int>{ 4, 3, 2, 2, 1, 1 });

            Assert::AreEqual<size_t>(2, list.remove(2));
            Assert::IsTrue(list == ff::forward_list<int>{ 4, 3, 1, 1 });

            Assert::AreEqual<size_t>(2, list.remove_if([](int i) { return i == 1; }));
            Assert::IsTrue(list == ff::forward_list<int>{ 4, 3 });

            Assert::AreEqual<size_t>(1, list.remove_if([](int i) { return i == 4; }));
            Assert::IsTrue(list == ff::forward_list<int>{ 3 });

            Assert::AreEqual<size_t>(1, list.remove_if([](int i) { return i == 3; }));
            Assert::IsTrue(list.empty());

            list.reverse();
            Assert::IsTrue(list.empty());
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
            ff::forward_list<int, SharedNodePool> list{ 1, 2, 3, 4, 5, 6, 7, 8 };

            list.splice_after(list.cbefore_begin(), list);
            Assert::IsTrue(list == ff::forward_list<int, SharedNodePool>{ 1, 2, 3, 4, 5, 6, 7, 8 });

            list.splice_after(list.cbefore_begin(), list, std::next(list.cbefore_begin(), 4));
            Assert::IsTrue(list == ff::forward_list<int, SharedNodePool>{ 5, 6, 7, 8, 1, 2, 3, 4 });

            ff::forward_list<int, SharedNodePool> list2;
            list2.splice_after(list2.cbefore_begin(), list, list.cbefore_begin(), std::next(list.cbefore_begin(), 5));
            Assert::IsTrue(list == ff::forward_list<int, SharedNodePool>{ 1, 2, 3, 4 });
            Assert::IsTrue(list2 == ff::forward_list<int, SharedNodePool>{ 5, 6, 7, 8 });

            list2.splice_after(list2.cbefore_begin(), list);
            Assert::IsTrue(list == ff::forward_list<int, SharedNodePool>{});
            Assert::IsTrue(list2 == ff::forward_list<int, SharedNodePool>{ 1, 2, 3, 4, 5, 6, 7, 8 });
        }
    };
}
