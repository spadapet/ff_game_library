#pragma once

#include "hash_set.h"

namespace ff
{
    template<class Key, class Hash = ff::hash<Key>, class KeyEqual = std::equal_to<Key>>
    class unordered_multiset
    {
    public:
        using this_type = typename unordered_multiset<Key, Hash, KeyEqual>;
        using internal_type = typename ff::internal::hash_set<Key, Hash, KeyEqual, true, true>;
        using key_type = typename Key;
        using value_type = typename Key;
        using size_type = typename size_t;
        using difference_type = typename ptrdiff_t;
        using hasher = typename Hash;
        using key_equal = typename KeyEqual;
        using reference = typename value_type&;
        using const_reference = typename const value_type&;
        using pointer = typename value_type*;
        using const_pointer = typename const value_type*;
        using const_iterator = typename internal_type::const_iterator;
        using iterator = typename internal_type::iterator;
        using const_dupe_iterator = typename internal_type::const_dupe_iterator;
        using dupe_iterator = typename internal_type::dupe_iterator;
        using const_local_iterator = typename internal_type::const_local_iterator;
        using local_iterator = typename internal_type::local_iterator;

        unordered_multiset()
        {
        }

        explicit unordered_multiset(size_type bucket_count)
            : data(bucket_count)
        {
        }

        template<class InputIt>
        unordered_multiset(InputIt first, InputIt last, size_type bucket_count = 0)
            : data(first, last, bucket_count)
        {
        }

        unordered_multiset(std::initializer_list<value_type> init, size_type bucket_count = 0)
            : data(init, bucket_count)
        {
        }

        unordered_multiset(const this_type& other)
            : data(other.data)
        {
        }

        unordered_multiset(this_type&& other)
            : data(std::move(other.data))
        {
        }

        ~unordered_multiset()
        {
        }

        this_type& operator=(const this_type& other)
        {
            this->data = other.data;
            return *this;
        }

        this_type& operator=(this_type&& other) noexcept
        {
            this->data = std::move(other.data);
            return *this;
        }

        this_type& operator=(std::initializer_list<value_type> ilist)
        {
            this->data = ilist;
            return *this;
        }

        // allocator_type get_allocator() const noexcept;

        iterator begin() noexcept
        {
            return this->data.cbegin();
        }

        const_iterator begin() const noexcept
        {
            return this->data.cbegin();
        }

        const_iterator cbegin() const noexcept
        {
            return this->data.cbegin();
        }

        iterator end() noexcept
        {
            return this->data.end();
        }

        const_iterator end() const noexcept
        {
            return this->data.end();
        }

        const_iterator cend() const noexcept
        {
            return this->data.cend();
        }

        local_iterator begin(size_type n)
        {
            return this->data.begin(n);
        }

        const_local_iterator begin(size_type n) const
        {
            return this->data.begin(n);
        }

        const_local_iterator cbegin(size_type n) const
        {
            return this->data.cbegin(n);
        }

        local_iterator end(size_type n)
        {
            return this->data.end(n);
        }

        const_local_iterator end(size_type n) const
        {
            return this->data.end(n);
        }

        const_local_iterator cend(size_type n) const
        {
            return this->data.cend(n);
        }

        bool empty() const noexcept
        {
            return this->data.empty();
        }

        size_type size() const noexcept
        {
            return this->data.size();
        }

        size_type max_size() const noexcept
        {
            return this->data.max_size();
        }

        void clear() noexcept
        {
            this->data.clear();
        }

        iterator insert(const value_type& value)
        {
            return this->data.insert(value).first;
        }

        iterator insert(value_type&& value)
        {
            return this->data.insert(std::move(value)).first;
        }

        iterator insert(const_iterator hint, const value_type& value)
        {
            return this->data.insert(hint, value).first;
        }

        iterator insert(const_iterator hint, value_type&& value)
        {
            return this->data.insert(hint, std::move(value)).first;
        }

        template<class InputIt>
        void insert(InputIt first, InputIt last)
        {
            this->data.insert(first, last);
        }

        void insert(std::initializer_list<value_type> ilist)
        {
            this->data.insert(ilist);
        }

        template<class... Args>
        iterator emplace(Args&&... args)
        {
            return this->data.emplace(0, std::forward<Args>(args)...).first;
        }

        template<class... Args>
        iterator emplace_hint(const_iterator hint, Args&&... args)
        {
            return this->data.emplace_hint(hint, std::forward<Args>(args)...).first;
        }

        template<class IterT>
        IterT erase(IterT pos)
        {
            return this->data.erase(pos);
        }

        template<class IterT>
        IterT erase(IterT first, IterT last)
        {
            return this->data.erase(first, last);
        }

        size_type erase(const key_type& key)
        {
            return this->data.erase(key);
        }

        void swap(this_type& other) noexcept
        {
            this->data.swap(other.data);
        }

        size_type count(const Key& key) const
        {
            return this->data.count(key);
        }

        iterator find(const Key& key)
        {
            return this->data.find(key);
        }

        const_iterator find(const Key& key) const
        {
            return this->data.find(key);
        }

        bool contains(const Key& key) const
        {
            return this->data.contains(key);
        }

        std::pair<dupe_iterator, dupe_iterator> equal_range(const Key& key)
        {
            return this->data.equal_range(key);
        }

        std::pair<const_dupe_iterator, const_dupe_iterator> equal_range(const Key& key) const
        {
            return this->data.equal_range(key);
        }

        size_type bucket_count() const
        {
            return this->data.bucket_count();
        }

        size_type max_bucket_count() const
        {
            return this->data.max_bucket_count();
        }

        size_type bucket_size(size_type n) const
        {
            return this->data.bucket_size(n);
        }

        size_type bucket(const Key& key) const
        {
            return this->data.bucket(key);
        }

        float load_factor() const
        {
            return this->data.load_factor();
        }

        float max_load_factor() const
        {
            return this->data.max_load_factor();
        }

        void max_load_factor(float ml)
        {
            this->data.max_load_factor(ml);
        }

        void rehash(size_type count)
        {
            this->data.rehash(count);
        }

        void reserve(size_type count)
        {
            this->data.reserve(count);
        }

        hasher hash_function() const
        {
            return this->data.hash_function();
        }

        key_equal key_eq() const
        {
            return this->data.key_eq();
        }

    private:
        internal_type data;
    };
}

namespace std
{
    template<class Key, class Hash, class KeyEqual>
    void swap(ff::unordered_multiset<Key, Hash, KeyEqual>& lhs, ff::unordered_multiset<Key, Hash, KeyEqual>& other) noexcept(noexcept(lhs.swap(other)))
    {
        lhs.swap(other);
    }
}
