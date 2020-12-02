#pragma once

#include "hash.h"
#include "list.h"
#include "vector.h"

namespace ff::internal
{
    template<class Key, bool AllowDupes>
    struct hash_set_node;

    template<class Key>
    struct hash_set_node<Key, true>
    {
        using this_type = hash_set_node<Key, true>;
        using list_iterator = typename ff::list<this_type>::iterator;
        using const_list_iterator = typename ff::list<this_type>::const_iterator;

        template<class... Args>
        hash_set_node(Args&&... args)
            : node_key(std::forward<Args>(args)...)
        {}

        Key node_key;
        size_t hash_key;
        list_iterator prev_node;
        list_iterator next_node;
        list_iterator prev_dupe;
        list_iterator next_dupe;
    };

    template<class Key>
    struct hash_set_node<Key, false>
    {
        using this_type = hash_set_node<Key, false>;
        using list_iterator = typename ff::list<this_type>::iterator;
        using const_list_iterator = typename ff::list<this_type>::const_iterator;

        template<class... Args>
        hash_set_node(Args&&... args)
            : node_key(std::forward<Args>(args)...)
            , hash_key(0)
        {}

        Key node_key;
        size_t hash_key;
        list_iterator prev_node;
        list_iterator next_node;
    };

    enum class hash_set_iterator_type
    {
        nodes,
        dupes,
        bucket,
    };

    template<class Key, bool AllowDupes, hash_set_iterator_type IteratorType, bool ConstKey>
    class hash_set_iterator
    {
    public:
        using this_type = typename hash_set_iterator<Key, AllowDupes, IteratorType, ConstKey>;
        using node_type = typename hash_set_node<Key, AllowDupes>;
        using list_iterator = typename node_type::list_iterator;
        using const_list_iterator = typename node_type::const_list_iterator;
        using iterator_category = typename std::forward_iterator_tag;
        using value_type = typename Key;
        using difference_type = typename ptrdiff_t;
        using pointer = typename value_type*;
        using reference = typename value_type&;

        hash_set_iterator(list_iterator iter)
            : iter(iter)
        {}

        template<hash_set_iterator_type IteratorType2>
        hash_set_iterator(const hash_set_iterator<Key, AllowDupes, IteratorType2, ConstKey>& other)
            : iter(other.internal_iter())
        {}

        template<hash_set_iterator_type IteratorType2>
        bool operator==(const hash_set_iterator<Key, AllowDupes, IteratorType2, ConstKey>& other)
        {
            return this->iter == other.internal_iter();
        }

        template<hash_set_iterator_type IteratorType2>
        bool operator!=(const hash_set_iterator<Key, AllowDupes, IteratorType2, ConstKey>& other)
        {
            return this->iter != other.internal_iter();
        }

        const_list_iterator internal_iter() const
        {
            return this->iter;
        }

        size_t internal_hash() const
        {
            return this->iter->hash_key;
        }

        reference operator*() const
        {
            return this->iter->node_key;
        }

        pointer operator->() const
        {
            return &this->iter->node_key;
        }

        this_type& operator++()
        {
            this->advance();
            return *this;
        }

        this_type operator++(int)
        {
            this_type pre = *this;
            this->advance();
            return pre;
        }

        bool operator==(const this_type& other) const
        {
            return this->iter == other.iter;
        }

        bool operator!=(const this_type& other) const
        {
            return this->iter != other.iter;
        }

    private:
        void advance()
        {
            if constexpr (IteratorType == hash_set_iterator_type::nodes || (!AllowDupes && IteratorType == hash_set_iterator_type::dupes))
            {
                ++this->iter;
            }
            else if constexpr (AllowDupes && IteratorType == hash_set_iterator_type::dupes)
            {
                this->iter = this->iter->next_dupe;
            }
            else if constexpr (IteratorType == hash_set_iterator_type::bucket)
            {
                this->iter = this->iter->next_node;
            }
        }

        list_iterator iter;
    };

    template<class Key, bool AllowDupes, hash_set_iterator_type IteratorType>
    class hash_set_iterator<Key, AllowDupes, IteratorType, true>
    {
    public:
        using this_type = typename hash_set_iterator<Key, AllowDupes, IteratorType, true>;
        using node_type = typename hash_set_node<Key, AllowDupes>;
        using list_iterator = typename node_type::list_iterator;
        using const_list_iterator = typename node_type::const_list_iterator;
        using iterator_category = typename std::forward_iterator_tag;
        using value_type = typename const Key;
        using difference_type = typename ptrdiff_t;
        using pointer = typename value_type*;
        using reference = typename value_type&;

        hash_set_iterator(list_iterator iter)
            : iter(iter)
        {}

        hash_set_iterator(const_list_iterator iter)
            : iter(iter)
        {}

        template<hash_set_iterator_type IteratorType2, bool ConstKey>
        hash_set_iterator(const hash_set_iterator<Key, AllowDupes, IteratorType2, ConstKey>& other)
            : iter(other.internal_iter())
        {}

        template<hash_set_iterator_type IteratorType2, bool ConstKey>
        bool operator==(const hash_set_iterator<Key, AllowDupes, IteratorType2, ConstKey>& other)
        {
            return this->iter == other.internal_iter();
        }

        template<hash_set_iterator_type IteratorType2, bool ConstKey>
        bool operator!=(const hash_set_iterator<Key, AllowDupes, IteratorType2, ConstKey>& other)
        {
            return this->iter != other.internal_iter();
        }

        const_list_iterator internal_iter() const
        {
            return this->iter;
        }

        size_t internal_hash() const
        {
            return this->iter->hash_key;
        }

        reference operator*() const
        {
            return this->iter->node_key;
        }

        pointer operator->() const
        {
            return &this->iter->node_key;
        }

        this_type& operator++()
        {
            this->advance();
            return *this;
        }

        this_type operator++(int)
        {
            this_type pre = *this;
            this->advance();
            return pre;
        }

        bool operator==(const this_type& other) const
        {
            return this->iter == other.iter;
        }

        bool operator!=(const this_type& other) const
        {
            return this->iter != other.iter;
        }

    private:
        void advance()
        {
            if constexpr (IteratorType == hash_set_iterator_type::nodes || (!AllowDupes && IteratorType == hash_set_iterator_type::dupes))
            {
                ++this->iter;
            }
            else if constexpr (AllowDupes && IteratorType == hash_set_iterator_type::dupes)
            {
                this->iter = this->iter->next_dupe;
            }
            else if constexpr (IteratorType == hash_set_iterator_type::bucket)
            {
                this->iter = this->iter->next_node;
            }
        }

        const_list_iterator iter;
    };

    template<class Key, class Hash, class KeyEqual, bool AllowDupes, bool ConstKey>
    class hash_set
    {
    public:
        using this_type = typename hash_set<Key, Hash, KeyEqual, AllowDupes, ConstKey>;
        using node_type = typename hash_set_node<Key, AllowDupes>;
        using list_iterator = typename node_type::list_iterator;
        using const_list_iterator = typename node_type::const_list_iterator;
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
        using const_iterator = typename hash_set_iterator<Key, AllowDupes, hash_set_iterator_type::nodes, true>;
        using iterator = typename hash_set_iterator<Key, AllowDupes, hash_set_iterator_type::nodes, ConstKey>;
        using const_dupe_iterator = typename hash_set_iterator<Key, AllowDupes, hash_set_iterator_type::dupes, true>;
        using dupe_iterator = typename hash_set_iterator<Key, AllowDupes, hash_set_iterator_type::dupes, ConstKey>;
        using const_local_iterator = typename hash_set_iterator<Key, AllowDupes, hash_set_iterator_type::bucket, true>;
        using local_iterator = typename hash_set_iterator<Key, AllowDupes, hash_set_iterator_type::bucket, ConstKey>;

        hash_set()
            : hash_set(0)
        {}

        explicit hash_set(size_type bucket_count)
            : load_size(0)
            , max_load_size(0)
            , max_load(2.0f)
        {
            this->rehash(bucket_count);
        }

        template<class InputIt>
        hash_set(InputIt first, InputIt last, size_type bucket_count = 0)
            : hash_set(bucket_count)
        {
            this->insert(first, last);
        }

        hash_set(std::initializer_list<value_type> init, size_type bucket_count = 0)
            : hash_set(bucket_count)
        {
            this->insert(init);
        }

        hash_set(const this_type& other)
            : hash_set()
        {
            *this = other;
        }

        hash_set(this_type&& other)
            : hash_set()
        {
            *this = std::move(other);
        }

        ~hash_set()
        {
            this->clear();
        }

        this_type& operator=(const this_type& other)
        {
            if (this != &other)
            {
                this->clear();
                this->insert(other.cbegin(), other.cend());
            }

            return *this;
        }

        this_type& operator=(this_type&& other) noexcept
        {
            this->swap(other);
            return *this;
        }

        this_type& operator=(std::initializer_list<value_type> ilist)
        {
            this->clear();
            this->insert(ilist.begin(), ilist.end());
        }

        // allocator_type get_allocator() const noexcept;

        iterator begin() noexcept
        {
            return this->nodes.cbegin();
        }

        const_iterator begin() const noexcept
        {
            return this->nodes.cbegin();
        }

        const_iterator cbegin() const noexcept
        {
            return this->nodes.cbegin();
        }

        iterator end() noexcept
        {
            return this->bad_node();
        }

        const_iterator end() const noexcept
        {
            return this->bad_node_const();
        }

        const_iterator cend() const noexcept
        {
            return this->bad_node_const();
        }

        local_iterator begin(size_type n)
        {
            return this->buckets[n];
        }

        const_local_iterator begin(size_type n) const
        {
            return this->buckets[n];
        }

        const_local_iterator cbegin(size_type n) const
        {
            return this->buckets[n];
        }

        local_iterator end(size_type n)
        {
            return this->bad_node();
        }

        const_local_iterator end(size_type n) const
        {
            return this->bad_node_const();
        }

        const_local_iterator cend(size_type n) const
        {
            return this->bad_node_const();
        }

        bool empty() const noexcept
        {
            return this->nodes.empty();
        }

        size_type size() const noexcept
        {
            return this->nodes.size();
        }

        size_type max_size() const noexcept
        {
            return this->nodes.max_size();
        }

        void clear() noexcept
        {
            this->nodes.clear();
            this->empty_buckets();
        }

        std::pair<iterator, bool> insert(const value_type& value, size_t hash = 0)
        {
            if constexpr (AllowDupes)
            {
                return this->emplace(hash, value);
            }
            else
            {
                if (!hash)
                {
                    hash = hasher()(value);
                }

                const_iterator i = this->find(value, hash);
                if (i == this->cend())
                {
                    list_iterator node = this->nodes.emplace(this->nodes.cend(), value);
                    node->hash_key = hash;
                    this->add_node_to_bucket(node, true);
                    return std::make_pair<iterator, bool>(std::move(node), true);
                }
                else
                {
                    return std::make_pair<iterator, bool>(i.internal_iter(), false);
                }
            }
        }

        std::pair<iterator, bool> insert(value_type&& value, size_t hash = 0)
        {
            if constexpr (AllowDupes)
            {
                return this->emplace(hash, std::move(value));
            }
            else
            {
                if (!hash)
                {
                    hash = hasher()(value);
                }

                const_iterator i = this->find(value, hash);
                if (i == this->cend())
                {
                    list_iterator node = this->nodes.emplace(this->nodes.cend(), std::move(value));
                    node->hash_key = hash;
                    this->add_node_to_bucket(node, true);
                    return std::make_pair<iterator, bool>(node, true);
                }
                else
                {
                    return std::make_pair<iterator, bool>(i.internal_iter(), false);
                }
            }
        }

        std::pair<iterator, bool> insert(const_iterator hint, const value_type& value)
        {
            return this->insert(value);
        }

        std::pair<iterator, bool> insert(const_iterator hint, value_type&& value)
        {
            return this->insert(std::move(value));
        }

        template<class InputIt>
        void insert(InputIt first, InputIt last)
        {
            for (; first != last; ++first)
            {
                this->insert(*first);
            }
        }

        void insert(std::initializer_list<value_type> ilist)
        {
            this->insert(ilist.begin(), ilist.end());
        }

        template<class... Args>
        std::pair<iterator, bool> emplace(size_t hash, Args&&... args)
        {
            list_iterator node = this->nodes.emplace(this->nodes.cend(), std::forward<Args>(args)...);
            node->hash_key = !hash ? hasher()(node->node_key) : hash;

            if constexpr (AllowDupes)
            {
                this->add_node_to_bucket(node, true);
                return std::make_pair<iterator, bool>(node, true);
            }
            else
            {
                iterator i = this->find(node->node_key, node->hash_key);
                if (i == this->end())
                {
                    this->add_node_to_bucket(node, true);
                    return std::make_pair<iterator, bool>(node, true);
                }
                else
                {
                    this->nodes.erase(node);
                    return std::make_pair<iterator, bool>(std::move(i), false);
                }
            }
        }

        template<class... Args>
        std::pair<iterator, bool> emplace_hint(const_iterator hint, Args&&... args)
        {
            return this->emplace(0, std::forward<Args>(args)...);
        }

        template<class IterT>
        IterT erase(IterT pos)
        {
            IterT pos2 = pos++;
            this->delete_node_from_bucket(pos2.internal_iter(), false);
            return pos;
        }

        template<class IterT>
        IterT erase(IterT first, IterT last)
        {
            while (first != last)
            {
                this->delete_node_from_bucket((first++).internal_iter(), false);
            }

            return last;
        }

        size_type erase(const key_type& key)
        {
            dupe_iterator i = this->find(key);
            return (i != this->bad_node()) ? this->delete_node_from_bucket(i.internal_iter(), true) : 0;
        }

        void swap(this_type& other) noexcept
        {
            if (this != &other)
            {
                std::swap(this->buckets, other.buckets);
                std::swap(this->nodes, other.nodes);
                std::swap(this->load_size, other.load_size);
                std::swap(this->max_load_size, other.max_load_size);
                std::swap(this->max_load, other.max_load);
            }
        }

        size_type count(const Key& key) const
        {
            size_t count = 0;
            std::pair<const_dupe_iterator, const_dupe_iterator> pair = this->equal_range(key);
            for (const_dupe_iterator i = pair.first; i != pair.second; ++i, ++count);
            return count;
        }

        iterator find(const Key& key)
        {
            if (!this->empty())
            {
                size_t hash = hasher()(key);
                for (list_iterator i = this->get_bucket_for_hash(hash); i != this->bad_node(); i = i->next_node)
                {
                    if (key_equal()(i->node_key, key))
                    {
                        return i;
                    }
                }
            }

            return this->end();
        }

        iterator find(const Key& key, size_t hash)
        {
            if (!this->buckets.empty())
            {
                list_iterator dupe = this->get_bucket_for_hash(hash);
                for (; dupe != this->bad_node(); dupe = dupe->next_node)
                {
                    if (dupe->hash_key == hash && key_equal()(dupe->node_key, key))
                    {
                        return dupe;
                    }
                }
            }

            return this->end();
        }

        const_iterator find(const Key& key) const
        {
            return const_cast<this_type*>(this)->find(key);
        }

        const_iterator find(const Key& key, size_t hash) const
        {
            return const_cast<this_type*>(this)->find(key, hash);
        }

        bool contains(const Key& key) const
        {
            return this->find(key) != this->cend();
        }

        bool contains(const Key& key, size_t hash) const
        {
            return this->find(key, hash) != this->cend();
        }

        std::pair<dupe_iterator, dupe_iterator> equal_range(const Key& key)
        {
            if constexpr (AllowDupes)
            {
                return std::make_pair<dupe_iterator, dupe_iterator>(this->find(key), this->end());
            }
            else
            {
                iterator i = this->find(key);
                return std::make_pair<dupe_iterator, dupe_iterator>(i, (i == this->end()) ? i : std::next(i));
            }
        }

        std::pair<const_dupe_iterator, const_dupe_iterator> equal_range(const Key& key) const
        {
            if constexpr (AllowDupes)
            {
                return std::make_pair<const_dupe_iterator, const_dupe_iterator>(this->find(key), this->cend());
            }
            else
            {
                const_iterator i = this->find(key);
                return std::make_pair<const_dupe_iterator, const_dupe_iterator>(i, (i == this->cend()) ? i : std::next(i));
            }
        }

        size_type bucket_count() const
        {
            return this->buckets.size();
        }

        size_type min_bucket_count() const
        {
            return 1 << 4;
        }

        size_type max_bucket_count() const
        {
            return 1 << 18;
        }

        size_type bucket_size(size_type n) const
        {
            size_type count = 0;
            for (const_local_iterator i = this->cbegin(n); i != this->cend(n); ++i, ++count);
            return count;
        }

        size_type bucket(const Key& key) const
        {
            this->reserve(1);
            size_t hash = hasher()(key);
            return hash & (this->buckets.size() - 1);
        }

        float load_factor() const
        {
            size_type bucket_count = this->buckets.size();
            return bucket_count ? this->load_size / static_cast<float>(bucket_count) : 0.0f;
        }

        float max_load_factor() const
        {
            return this->max_load;
        }

        void max_load_factor(float ml)
        {
            this->max_load = ml;
            this->update_max_load_size();
            this->reserve(this->load_size);
        }

        bool rehash(size_type count)
        {
            if (count > this->buckets.size())
            {
                size_type min_buckets = (this->max_load > 0.0f) ? static_cast<size_type>(std::ceil(this->load_size / this->max_load)) : 0;
                return this->set_bucket_count(std::max(min_buckets, count));
            }

            return false;
        }

        bool reserve(size_type count)
        {
            if (count > this->max_load_size)
            {
                size_type min_buckets = (this->max_load > 0.0f) ? static_cast<size_type>(std::ceil(count / this->max_load)) : 0;
                return this->rehash(min_buckets);
            }

            return false;
        }

        hasher hash_function() const
        {
            return hasher();
        }

        key_equal key_eq() const
        {
            return key_equal();
        }

    private:
        void empty_buckets()
        {
            this->load_size = 0;

            size_type count = this->buckets.size();
            for (size_type i = 0; i < count; i++)
            {
                this->buckets[i] = this->bad_node();
            }
        }

        bool set_bucket_count(size_type count)
        {
            count = ff::math::nearest_power_of_two(count);
            count = std::max<size_type>(count, this->min_bucket_count());
            count = std::min<size_type>(count, this->max_bucket_count());

            if (count > this->buckets.size())
            {
                this->buckets.resize(count);
                this->empty_buckets();
                this->update_max_load_size();

                for (list_iterator i = this->nodes.begin(); i != this->nodes.end(); ++i)
                {
                    this->add_node_to_bucket(i, false);
                }

                return true;
            }

            return false;
        }

        void update_max_load_size()
        {
            this->max_load_size = (this->buckets.size() && this->max_load > 0.0f) ? static_cast<size_type>(this->buckets.size() * this->max_load) : 0;
        }

        void add_node_to_bucket(list_iterator node, bool allow_bucket_resize)
        {
            list_iterator* bucket = !this->buckets.empty() ? &this->get_bucket_for_hash(node->hash_key) : nullptr;

            if constexpr (AllowDupes)
            {
                list_iterator dupe = bucket ? *bucket : this->bad_node();
                for (; dupe != this->bad_node(); dupe = dupe->next_node)
                {
                    if (dupe->hash_key == node->hash_key && key_equal()(dupe->node_key, node->node_key))
                    {
                        break;
                    }
                }

                if (dupe != this->bad_node())
                {
                    // Add to the top of the stack of dupes
                    node->next_node = dupe->next_node;
                    node->prev_node = dupe->prev_node;
                    node->next_dupe = dupe;
                    node->prev_dupe = this->bad_node();
                    dupe->prev_dupe = node;

                    if (dupe->next_node != this->bad_node())
                    {
                        dupe->next_node->prev_node = node;
                        dupe->next_node = this->bad_node();
                    }

                    if (dupe->prev_node != this->bad_node())
                    {
                        dupe->prev_node->next_node = node;
                        dupe->prev_node = this->bad_node();
                    }
                    else
                    {
                        *bucket = node;
                    }

                    return;
                }
            }

            if (!allow_bucket_resize || !this->reserve(this->load_size + 1))
            {
                assert(bucket);
                if (bucket)
                {
                    if (*bucket != this->bad_node())
                    {
                        (*bucket)->prev_node = node;
                    }

                    node->next_node = *bucket;
                    node->prev_node = this->bad_node();

                    if constexpr (AllowDupes)
                    {
                        node->next_dupe = this->bad_node();
                        node->prev_dupe = this->bad_node();
                    }

                    *bucket = node;

                    this->load_size++;
                }
            }
        }

        size_type delete_node_from_bucket(const_list_iterator node, bool include_dupes)
        {
            if constexpr (AllowDupes)
            {
                if (include_dupes)
                {
                    while (node->prev_dupe != this->bad_node())
                    {
                        node = node->prev_dupe;
                    }

                    if (node->prev_node != this->bad_node())
                    {
                        node->prev_node->next_node = node->next_node;
                    }
                    else
                    {
                        this->get_bucket_for_hash(node->hash_key) = node->next_node;
                    }

                    if (node->next_node != this->bad_node())
                    {
                        node->next_node->prev_node = node->prev_node;
                    }

                    size_t count = 0;
                    for (const_list_iterator dupe; node != this->bad_node_const(); node = dupe)
                    {
                        dupe = node->next_dupe;
                        this->nodes.erase(node);
                        count++;
                    }

                    this->load_size--;

                    return count;
                }

                if (node->next_dupe != this->bad_node())
                {
                    node->next_dupe->prev_dupe = node->prev_dupe;
                }

                if (node->prev_dupe != this->bad_node())
                {
                    node->prev_dupe->next_dupe = node->next_dupe;
                }
                else
                {
                    if (node->prev_node != this->bad_node())
                    {
                        if (node->next_dupe != this->bad_node())
                        {
                            node->prev_node->next_node = node->next_dupe;
                            node->next_dupe->prev_node = node->prev_node;
                        }
                        else
                        {
                            node->prev_node->next_node = node->next_node;
                        }
                    }
                    else
                    {
                        this->get_bucket_for_hash(node->hash_key) = (node->next_dupe != this->bad_node()) ? node->next_dupe : node->next_node;
                    }

                    if (node->next_node != this->bad_node())
                    {
                        if (node->next_dupe != this->bad_node())
                        {
                            node->next_node->prev_node = node->next_dupe;
                            node->next_dupe->next_node = node->next_node;
                        }
                        else
                        {
                            node->next_node->prev_node = node->prev_node;
                        }
                    }

                    if (node->next_dupe == this->bad_node())
                    {
                        this->load_size--;
                    }
                }
            }
            else
            {
                if (node->prev_node != this->bad_node())
                {
                    node->prev_node->next_node = node->next_node;
                }
                else
                {
                    this->get_bucket_for_hash(node->hash_key) = node->next_node;
                }

                if (node->next_node != this->bad_node())
                {
                    node->next_node->prev_node = node->prev_node;
                }

                this->load_size--;
            }

            this->nodes.erase(node);
            return 1;
        }

        list_iterator& get_bucket_for_hash(size_t hash)
        {
            return this->buckets[hash & (this->buckets.size() - 1)];
        }

        const_list_iterator get_bucket_for_hash_const(size_t hash) const
        {
            return this->buckets[hash & (this->buckets.size() - 1)];
        }

        list_iterator bad_node()
        {
            return this->nodes.end();
        }

        const_list_iterator bad_node_const() const
        {
            return this->nodes.cend();
        }

        ff::vector<list_iterator> buckets;
        ff::list<node_type> nodes;
        size_type load_size;
        size_type max_load_size;
        float max_load;
    };
}
