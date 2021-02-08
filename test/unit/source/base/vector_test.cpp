#include "pch.h"

namespace base_test
{
    TEST_CLASS(vector_test)
    {
    public:
        TEST_METHOD(swap_alloc)
        {
            ff::stack_vector<int> v1{ 1, 2, 3, 4, 5, 6, 7, 8 };
            ff::stack_vector<int> v2{ 10, 20, 30, 40 };

            const int* d1 = v1.data();
            const int* d2 = v2.data();

            std::swap(v1, v2);

            Assert::AreEqual<const int*>(d2, v1.data());
            Assert::AreEqual<const int*>(d1, v2.data());
            Assert::AreEqual<size_t>(4, v1.size());
            Assert::AreEqual<size_t>(8, v2.size());
            Assert::IsTrue(v1.capacity() >= v1.size());
            Assert::IsTrue(v2.capacity() >= v2.size());

            auto foo = typeid(v1).name();
            auto foo2 = typeid(v1).raw_name();
            Assert::AreNotEqual(foo, foo2);
        }

        TEST_METHOD(count_alloc)
        {
            ff::stack_vector<int> v1(7);
            ff::stack_vector<int> v2(7, 2);
            ff::stack_vector<int> v3(v2);
            ff::stack_vector<int> v4(v2.cbegin(), v2.cend());

            std::memcpy(v1.data(), v2.data(), sizeof(int) * v2.size());
            Assert::IsTrue(v1 == v2);
            Assert::IsTrue(v1 == v3);
            Assert::IsTrue(v1 == v4);

            Assert::IsTrue(v1.capacity() > v1.size());
            v1.shrink_to_fit();
            Assert::IsTrue(v1.capacity() > v1.size());
        }

        TEST_METHOD(iterators)
        {
            ff::stack_vector<int> v1{ 1, 2, 3, 4, 5, 6 };
            const ff::stack_vector<int> v2{ 1, 2, 3, 4, 5, 6 };
            ff::stack_vector<int> v3(v1.crbegin(), v1.crend());

            Assert::AreEqual(1, *v2.begin());
            Assert::AreEqual(3, *(v2.begin() + 2));
            Assert::AreEqual(6, *(v2.end() - 1));
            Assert::AreEqual(1, v2.front());
            Assert::AreEqual(6, v2.back());
            Assert::AreEqual(v2.data() , &*v2.begin());

            Assert::AreEqual(1, *v1.begin());
            Assert::AreEqual(3, *(v1.begin() + 2));
            Assert::AreEqual(6, *(v1.end() - 1));
            Assert::AreEqual(1, v1.front());
            Assert::AreEqual(6, v1.back());
            Assert::AreEqual(v1.data(), &*v1.begin());
            Assert::AreEqual(v1.size(), static_cast<size_t>(std::distance(v1.cbegin(), v1.cend())));

            Assert::IsTrue(v3 == ff::stack_vector<int>{ 6, 5, 4, 3, 2, 1 });
        }

        TEST_METHOD(insert_item)
        {
            ff::stack_vector<std::pair<int, char>, 6> v1{ { 1, 'a' }, { 2, 'b' }, { 3, 'c' }, { 4, 'd' } };
            ff::stack_vector<std::pair<int, char>, 6> v2{ { 1, 'a' }, { 8, 'h' }, { 7, 'g' }, { 2, 'b' }, { 3, 'c' }, { 4, 'd' }, { 5, 'e' } };
            ff::stack_vector<std::pair<int, char>, 6> v3{ { 1, 'a' }, { 5, 'e' } };

            Assert::AreEqual<size_t>(6, v1.capacity());

            v1.push_back(std::make_pair(5, 'e'));
            v1.emplace_back(6, 'f');
            auto i = v1.emplace(v1.cbegin() + 1, 7, 'g');
            v1.insert(i, std::make_pair(8, 'h'));

            Assert::AreEqual<size_t>(8, v1.size());
            Assert::IsTrue(v1.capacity() >= v1.size());

            v1.pop_back();
            Assert::AreEqual<size_t>(7, v1.size());

            Assert::IsTrue(v1 == v2);
            i = v1.erase(v1.cbegin() + 1, v1.cend() - 1);
            Assert::IsTrue(v1 == v3);

            v1.shrink_to_fit();
            Assert::AreEqual<size_t>(6, v1.capacity());
        }
    };
}
