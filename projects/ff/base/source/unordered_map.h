#pragma once

#include "hash_set.h"

namespace ff
{
    template<class Key, class T, class Hash = ff::hash<Key>, class KeyEqual = std::equal_to<Key>>
    class unordered_map
    {
    public:
        using this_type = typename unordered_map<Key, T, Hash, KeyEqual>;
        using key_type = typename Key;
        using mapped_type = typename T;
        using value_type = typename std::pair<const Key, T>;
        using internal_type = typename ff::internal::hash_set<value_type, ff::internal::pair_key_hash<Hash, Key, T>, ff::internal::pair_key_compare<std::equal_to<Key>, Key, T>, false, false>;
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

        unordered_map()
        {}

        explicit unordered_map(size_type bucket_count)
            : data(bucket_count)
        {}

        template<class InputIt>
        unordered_map(InputIt first, InputIt last, size_type bucket_count = 0)
            : data(first, last, bucket_count)
        {}

        unordered_map(std::initializer_list<value_type> init, size_type bucket_count = 0)
            : data(init, bucket_count)
        {}

        unordered_map(const this_type& other)
            : data(other.data)
        {}

        unordered_map(this_type&& other)
            : data(std::move(other.data))
        {}

        ~unordered_map()
        {}

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

        std::pair<iterator, bool> insert(const value_type& value)
        {
            return this->data.insert(value);
        }

        std::pair<iterator, bool> insert(value_type&& value)
        {
            return this->data.insert(std::move(value));
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

        template<class M>
        std::pair<iterator, bool> insert_or_assign(const key_type& k, M&& obj)
        {
            size_t hash = hasher()(k);
            iterator i = this->data.find(this_type::fake_value(k), hash);

            if (i != this->data.end())
            {
                i->second = std::forward<M>(obj);
                return std::make_pair<iterator, bool>(std::move(i), false);
            }

            return this->data.emplace(hash, k, std::forward<M>(obj));
        }

        template<class M>
        std::pair<iterator, bool> insert_or_assign(key_type&& k, M&& obj)
        {
            size_t hash = hasher()(k);
            iterator i = this->data.find(this_type::fake_value(k), hash);

            if (i != this->data.end())
            {
                i->second = std::forward<M>(obj);
                return std::make_pair<iterator, bool>(std::move(i), false);
            }

            return this->data.emplace(hash, std::move(k), std::forward<M>(obj));
        }

        template<class M>
        iterator insert_or_assign(const_iterator hint, const key_type& k, M&& obj)
        {
            return this->insert_or_assign(k, std::forward<M>(obj)).first;
        }

        template<class M>
        iterator insert_or_assign(const_iterator hint, key_type&& k, M&& obj)
        {
            return this->insert_or_assign(std::move(k), std::forward<M>(obj)).first;
        }

        template<class... Args>
        std::pair<iterator, bool> emplace(Args&&... args)
        {
            return this->data.emplace(0, std::forward<Args>(args)...);
        }

        template<class... Args>
        std::pair<iterator, bool> emplace_hint(const_iterator hint, Args&&... args)
        {
            return this->data.emplace_hint(hint, std::forward<Args>(args)...);
        }

        template<class... Args>
        std::pair<iterator, bool> try_emplace(const key_type& k, Args&&... args)
        {
            size_t hash = hasher()(k);
            iterator i = this->data.find(this_type::fake_value(k), hash);

            if (i != this->data.end())
            {
                return std::make_pair<iterator, bool>(std::move(i), false);
            }

            return this->data.emplace(hash, value_type(std::piecewise_construct, std::forward_as_tuple(k), std::forward_as_tuple(std::forward<Args>(args)...)));
        }

        template<class... Args>
        std::pair<iterator, bool> try_emplace(key_type&& k, Args&&... args)
        {
            size_t hash = hasher()(k);
            iterator i = this->data.find(this_type::fake_value(k), hash);

            if (i != this->data.end())
            {
                return std::make_pair<iterator, bool>(std::move(i), false);
            }

            return this->data.emplace(hash, value_type(std::piecewise_construct, std::forward_as_tuple(std::move(k)), std::forward_as_tuple(std::forward<Args>(args)...)));
        }

        template<class... Args>
        iterator try_emplace(const_iterator hint, const key_type& k, Args&&... args)
        {
            return this->try_emplace(k, std::forward<Args>(args)...).first;
        }

        template<class... Args>
        iterator try_emplace(const_iterator hint, key_type&& k, Args&&... args)
        {
            return this->try_emplace(std::move(k), std::forward<Args>(args)...).first;
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
            return this->data.erase(this_type::fake_value(key));
        }

        void swap(this_type& other) noexcept
        {
            this->data.swap(other.data);
        }

        T& at(const Key& key)
        {
            iterator i = this->data.find(this_type::fake_value(key));
            assert(i != this->data.end());
            return i->second;
        }

        const T& at(const Key& key) const
        {
            const_iterator i = this->data.find(this_type::fake_value(key));
            assert(i != this->data.cend());
            return i->second;
        }

        T& operator[](const Key& key)
        {
            return this->try_emplace(key).first->second;
        }

        T& operator[](Key&& key)
        {
            return this->try_emplace(std::move(key)).first->second;
        }

        size_type count(const Key& key) const
        {
            return this->data.count(this_type::fake_value(key));
        }

        iterator find(const Key& key)
        {
            return this->data.find(this_type::fake_value(key));
        }

        const_iterator find(const Key& key) const
        {
            return this->data.find(this_type::fake_value(key));
        }

        bool contains(const Key& key) const
        {
            return this->data.contains(this_type::fake_value(key));
        }

        std::pair<dupe_iterator, dupe_iterator> equal_range(const Key& key)
        {
            return this->data.equal_range(this_type::fake_value(key));
        }

        std::pair<const_dupe_iterator, const_dupe_iterator> equal_range(const Key& key) const
        {
            return this->data.equal_range(this_type::fake_value(key));
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
            return this->data.bucket(this_type::fake_value(key));
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
        static const value_type& fake_value(const key_type& k)
        {
            return *reinterpret_cast<const value_type*>(&k);
        }

        internal_type data;
    };
}

namespace std
{
    template<class Key, class Hash, class KeyEqual>
    void swap(ff::unordered_map<Key, Hash, KeyEqual>& lhs, ff::unordered_map<Key, Hash, KeyEqual>& other) noexcept(noexcept(lhs.swap(other)))
    {
        lhs.swap(other);
    }
}
