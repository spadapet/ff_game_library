#pragma once

#include "pool_allocator.h"
#include "type_helper.h"

namespace ff::internal
{
    template<class T>
    struct list_node
    {
        using this_type = typename list_node<T>;

        template<class... Args>
        list_node(this_type* prev, this_type* next, Args&&... args)
            : value(std::forward<Args>(args)...)
            , prev(prev)
            , next(next)
        {}

        T value;
        this_type* prev;
        this_type* next;
    };

    template<class T, bool SharedNodePool>
    class list_node_pool
    {
    protected:
        using node_pool_type = typename ff::pool_allocator<ff::internal::list_node<T>, false>;

        node_pool_type& get_node_pool()
        {
            return this->node_pool;
        }

        size_t get_size() const
        {
            size_t size;
            this->node_pool.get_stats(&size, nullptr);
            return size;
        }

        void add_size(size_t count) const {}
        void sub_size(size_t count) const {}
        void swap_size(list_node_pool<T, SharedNodePool>& other) const {}

    private:
        node_pool_type node_pool;
    };

    template<class T>
    class list_node_pool<T, true>
    {
    protected:
        using node_pool_type = typename ff::pool_allocator<ff::internal::list_node<T>, false>;

        list_node_pool()
            : item_size(0)
        {}

        static node_pool_type& get_node_pool()
        {
            static node_pool_type node_pool;
            return node_pool;
        }

        size_t get_size() const
        {
            return this->item_size;
        }

        void add_size(size_t count)
        {
            this->item_size += count;
        }

        void sub_size(size_t count)
        {
            assert(this->item_size >= count);
            this->item_size -= count;
        }

        void swap_size(list_node_pool<T, true>& other)
        {
            std::swap(this->item_size, other.item_size);
        }

    private:
        size_t item_size;
    };

    template<class T>
    class list_iterator
    {
    public:
        using this_type = typename list_iterator<T>;
        using node_type = typename list_node<typename std::remove_const_t<T>>;
        using iterator_category = typename std::bidirectional_iterator_tag;
        using value_type = typename T;
        using difference_type = typename ptrdiff_t;
        using pointer = typename T*;
        using reference = typename T&;

        list_iterator()
            : node(nullptr)
        {}

        list_iterator(node_type* node)
            : node(node)
        {}

        list_iterator(const this_type& other)
            : node(other.node)
        {}

        // Convert non-const iterator to const iterator
        template<class = std::enable_if_t<std::is_const_v<T>>>
        list_iterator(const list_iterator<std::remove_const_t<T>>& other)
            : node(other.internal_node())
        {}

        node_type* internal_node() const
        {
            return this->node;
        }

        reference operator*() const
        {
            assert(this->node);
            return this->node->value;
        }

        pointer operator->() const
        {
            assert(this->node);
            return &this->node->value;
        }

        this_type& operator++()
        {
            assert(this->node);
            this->node = this->node->next;
            return *this;
        }

        this_type operator++(int)
        {
            assert(this->node);
            this_type pre = *this;
            this->node = this->node->next;
            return pre;
        }

        this_type& operator--()
        {
            assert(this->node);
            this->node = this->node->prev;
            return *this;
        }

        this_type operator--(int)
        {
            assert(this->node);
            this_type pre = *this;
            this->node = this->node->prev;
            return pre;
        }

        bool operator==(const list_iterator<typename std::remove_const_t<T>>& other) const
        {
            return this->node == other.internal_node();
        }

        bool operator==(const list_iterator<typename std::add_const_t<T>>& other) const
        {
            return this->node == other.internal_node();
        }

        bool operator!=(const list_iterator<typename std::remove_const_t<T>>& other) const
        {
            return this->node != other.internal_node();
        }

        bool operator!=(const list_iterator<typename std::add_const_t<T>>& other) const
        {
            return this->node != other.internal_node();
        }

    private:
        node_type* node;
    };
}

namespace ff
{
    /// <summary>
    /// Replacement class for std::list
    /// </summary>
    /// <remarks>
    /// This is a replacement for the std::list class, but it always uses a memory pool for items.
    /// </remarks>
    /// <typeparam name="T">Item type</typeparam>
    /// <typeparam name="SharedNodePool">True to share the node pool among all lists of same type</typeparam>
    template<class T, bool SharedNodePool = false>
    class list : private ff::internal::list_node_pool<T, SharedNodePool>
    {
    public:
        using this_type = typename list<T, SharedNodePool>;
        using node_type = typename ff::internal::list_node<T>;
        using value_type = typename T;
        using size_type = typename size_t;
        using difference_type = typename ptrdiff_t;
        using reference = typename value_type&;
        using const_reference = typename const value_type&;
        using pointer = typename T*;
        using const_pointer = typename const T*;
        using iterator = typename ff::internal::list_iterator<T>;
        using const_iterator = typename ff::internal::list_iterator<const T>;
        using reverse_iterator = typename std::reverse_iterator<iterator>;
        using const_reverse_iterator = typename std::reverse_iterator<const_iterator>;

        list()
            : head_node(nullptr)
            , tail_node(nullptr)
        {}

        list(size_type count, const T& value)
            : list()
        {
            this->resize(count, value);
        }

        explicit list(size_type count)
            : list()
        {
            this->resize(count);
        }

        template<class InputIt, std::enable_if_t<ff::internal::is_iterator_t<InputIt>, int> = 0>
        list(InputIt first, InputIt last)
            : list()
        {
            this->assign(first, last);
        }

        list(const this_type& other)
            : list()
        {
            *this = other;
        }

        list(this_type&& other)
            : list()
        {
            *this = std::move(other);
        }

        list(std::initializer_list<T> init)
            : list()
        {
            this->assign(init);
        }

        ~list()
        {
            this->clear();
        }

        list& operator=(const this_type& other)
        {
            if (this != &other)
            {
                this->assign(other.cbegin(), other.cend());
            }

            return *this;
        }

        list& operator=(this_type&& other) noexcept
        {
            if (this != &other)
            {
                this->clear();
                this->get_node_pool() = std::move(other.get_node_pool());
                this->swap_size(other);
                std::swap(this->head_node, other.head_node);
                std::swap(this->tail_node, other.tail_node);
            }

            return *this;
        }

        list& operator=(std::initializer_list<T> ilist)
        {
            this->assign(ilist);
            return *this;
        }

        void assign(size_type count, const T& value)
        {
            this->clear();
            this->resize(count, value);
        }

        template<class InputIt, std::enable_if_t<ff::internal::is_iterator_t<InputIt>, int> = 0>
        void assign(InputIt first, InputIt last)
        {
            this->clear();
            this->insert(this->cend(), first, last);
        }

        void assign(std::initializer_list<T> ilist)
        {
            this->clear();
            this->insert(this->cend(), ilist);
        }

        // allocator_type get_allocator() const noexcept

        reference front()
        {
            assert(!this->empty());
            return this->head_node->value;
        }

        const_reference front() const
        {
            assert(!this->empty());
            return this->head_node->value;
        }

        reference back()
        {
            assert(!this->empty());
            return this->tail_node->value;
        }

        const_reference back() const
        {
            assert(!this->empty());
            return this->tail_node->value;
        }

        iterator begin() noexcept
        {
            return iterator(this->head_node);
        }

        const_iterator begin() const noexcept
        {
            return this->cbegin();
        }

        const_iterator cbegin() const noexcept
        {
            return const_iterator(this->head_node);
        }

        iterator end() noexcept
        {
            return iterator(nullptr);
        }

        const_iterator end() const noexcept
        {
            return this->cend();
        }

        const_iterator cend() const noexcept
        {
            return const_iterator(nullptr);
        }

        reverse_iterator rbegin() noexcept
        {
            return reverse_iterator(this->end());
        }

        const_reverse_iterator rbegin() const noexcept
        {
            return const_reverse_iterator(this->cend());
        }

        const_reverse_iterator crbegin() const noexcept
        {
            return const_reverse_iterator(this->cend());
        }

        reverse_iterator rend() noexcept
        {
            return reverse_iterator(this->begin());
        }

        const_reverse_iterator rend() const noexcept
        {
            return const_reverse_iterator(this->cbegin());
        }

        const_reverse_iterator crend() const noexcept
        {
            return const_reverse_iterator(this->cbegin());
        }

        bool empty() const noexcept
        {
            return this->head_node == nullptr;
        }

        size_type size() const noexcept
        {
            return this->get_size();
        }

        size_type max_size() const noexcept
        {
            return std::numeric_limits<size_type>::max();
        }

        void clear() noexcept
        {
            this->erase(this->cbegin(), this->cend());
        }

        iterator insert(const_iterator pos, const T& value)
        {
            return this->emplace(pos, value);
        }

        iterator insert(const_iterator pos, T&& value)
        {
            return this->emplace(pos, std::move(value));
        }

        iterator insert(const_iterator pos, size_type count, const T& value)
        {
            for (; count; count--)
            {
                pos = this->insert(pos, value);
            }

            return iterator(pos.internal_node());
        }

        template<class InputIt, std::enable_if_t<ff::internal::is_iterator_t<InputIt>, int> = 0>
        iterator insert(const_iterator pos, InputIt first, InputIt last)
        {
            if (first == last)
            {
                return iterator(pos.internal_node());
            }

            iterator first_insert = this->insert(pos, *first++);

            while (first != last)
            {
                this->insert(pos, *first++);
            }

            return first_insert;
        }

        iterator insert(const_iterator pos, std::initializer_list<T> ilist)
        {
            return this->insert(pos, ilist.begin(), ilist.end());
        }

        template<class... Args>
        iterator emplace(const_iterator pos, Args&&... args)
        {
            node_type* after_node = pos.internal_node();
            node_type* before_node = after_node ? after_node->prev : this->tail_node;
            node_type* node = this->get_node_pool().new_obj(before_node, after_node, std::forward<Args>(args)...);

            node_type*& set_next = before_node ? before_node->next : this->head_node;
            set_next = node;

            node_type*& set_prev = after_node ? after_node->prev : this->tail_node;
            set_prev = node;

            this->add_size(1);

            return iterator(node);
        }

        iterator erase(const_iterator pos)
        {
            assert(!this->empty() && pos != this->cend());
            return this->erase(pos, std::next(pos));
        }

        iterator erase(const_iterator first, const_iterator last)
        {
            node_type* node = first.internal_node();

            if (first != last)
            {
                node_type*& set_next = first.internal_node()->prev ? first.internal_node()->prev->next : this->head_node;
                set_next = last.internal_node();

                node_type*& set_prev = last.internal_node() ? last.internal_node()->prev : this->tail_node;
                set_prev = first.internal_node()->prev;

                size_t count = 0;
                for (node_type* node = first.internal_node(), *next; node != last.internal_node(); node = next, ++count)
                {
                    next = node->next;
                    this->get_node_pool().delete_obj(node);
                }

                this->sub_size(count);

            }

            return iterator(last.internal_node());
        }

        void push_back(const T& value)
        {
            this->emplace_back(value);
        }

        void push_back(T&& value)
        {
            this->emplace_back(std::move(value));
        }

        template<class... Args>
        reference emplace_back(Args&&... args)
        {
            return *this->emplace(this->cend(), std::forward<Args>(args)...);
        }

        void pop_back()
        {
            assert(!this->empty());
            this->erase(const_iterator(this->tail_node), this->cend());
        }

        void push_front(const T& value)
        {
            this->emplace_front(value);
        }

        void push_front(T&& value)
        {
            this->emplace_front(std::move(value));
        }

        template<class... Args>
        reference emplace_front(Args&&... args)
        {
            return *this->emplace(this->cbegin(), std::forward<Args>(args)...);
        }

        void pop_front()
        {
            assert(!this->empty());
            this->erase(this->cbegin(), std::next(this->cbegin()));
        }

        void resize(size_type count)
        {
            while (this->size() < count)
            {
                this->emplace_back();
            }

            if (this->size() > count)
            {
                this->erase(std::prev(const_iterator(this->tail_node), this->size() - count - 1), this->cend());
            }
        }

        void resize(size_type count, const value_type& value)
        {
            if (this->size() < count)
            {
                this->insert(this->cend(), count - this->size(), value);
            }

            if (this->size() > count)
            {
                this->erase(std::prev(const_iterator(this->tail_node), this->size() - count - 1), this->cend());
            }
        }

        void swap(this_type& other) noexcept
        {
            if (this != &other)
            {
                std::swap(this->get_node_pool(), other.get_node_pool());
                std::swap(this->head_node, other.head_node);
                std::swap(this->tail_node, other.tail_node);
                this->swap_size(other);
            }
        }

        // Probably not going to deal with sorted linked lists
        //
        // void merge(this_type& other);
        // void merge(this_type&& other);
        // template<class Compare>
        // void merge(this_type& other, Compare comp);
        // template <class Compare>
        // void merge(this_type&& other, Compare comp);
        // size_type unique();
        // template<class BinaryPredicate>
        // size_type unique(BinaryPredicate p);
        // void sort();
        // template<class Compare>
        // void sort(Compare comp);

        void splice(const_iterator pos, this_type& other)
        {
            this->splice(pos, std::move(other), other.cbegin(), other.cend());
        }

        void splice(const_iterator pos, this_type&& other)
        {
            this->splice(pos, std::move(other), other.cbegin(), other.cend());
        }

        void splice(const_iterator pos, this_type& other, const_iterator it)
        {
            this->splice(pos, std::move(other), it, other.cend());
        }

        void splice(const_iterator pos, this_type&& other, const_iterator it)
        {
            this->splice(pos, std::move(other), it, other.cend());
        }

        void splice(const_iterator pos, this_type& other, const_iterator first, const_iterator last)
        {
            this->splice(pos, std::move(other), first, last);
        }

        void splice(const_iterator pos, this_type&& other, const_iterator first, const_iterator last)
        {
            if (&this->get_node_pool() == &other.get_node_pool())
            {
                if (first != last && (this != &other || (first != pos && last != pos)))
                {
                    if (this != &other)
                    {
                        size_type count = std::distance(first, last);
                        this->add_size(count);
                        other.sub_size(count);
                    }

                    // Take the nodes out of other

                    node_type*& update_old_next = first.internal_node()->prev ? first.internal_node()->prev->next : other.head_node;
                    update_old_next = last.internal_node();

                    const_iterator old_before_last = (last != other.cend()) ? std::prev(last) : const_iterator(other.tail_node);
                    node_type*& update_old_prev = old_before_last.internal_node()->next ? old_before_last.internal_node()->next->prev : other.tail_node;
                    update_old_prev = first.internal_node()->prev;

                    // Add the nodes into this

                    const_iterator new_before_pos = pos.internal_node() ? std::prev(pos) : const_iterator(this->tail_node);
                    first.internal_node()->prev = new_before_pos.internal_node();
                    old_before_last.internal_node()->next = pos.internal_node();

                    node_type*& update_new_next = new_before_pos.internal_node() ? new_before_pos.internal_node()->next : this->head_node;
                    update_new_next = first.internal_node();

                    node_type*& update_new_prev = pos.internal_node() ? pos.internal_node()->prev : this->tail_node;
                    update_new_prev = old_before_last.internal_node();
                }
            }
            else // Not possible to really splice, since the node pools are different. Just insert/delete instead.
            {
                for (iterator cur = first.internal_node(), last2 = last.internal_node(); cur != last2; ++cur)
                {
                    this->emplace(pos, std::move(*cur));
                }

                other.erase(first, last);
            }
        }

        size_type remove(const T& value)
        {
            size_type removed = 0;
            for (const_iterator cur = this->cbegin(); cur != this->cend(); )
            {
                if (value == *cur)
                {
                    cur = this->erase(cur);
                    removed++;
                }
                else
                {
                    ++cur;
                }
            }

            return removed;
        }

        template<class UnaryPredicate>
        size_type remove_if(UnaryPredicate p)
        {
            size_type removed = 0;
            for (const_iterator cur = this->cbegin(); cur != this->cend(); )
            {
                if (p(*cur))
                {
                    cur = this->erase(cur);
                    removed++;
                }
                else
                {
                    ++cur;
                }
            }

            return removed;
        }

        void reverse() noexcept
        {
            for (node_type* node = this->head_node; node; node = node->prev)
            {
                std::swap(node->next, node->prev);
            }

            std::swap(this->head_node, this->tail_node);
        }

    private:
        node_type* head_node;
        node_type* tail_node;
    };
}

template<class T, bool SharedNodePool>
bool operator==(const ff::list<T, SharedNodePool>& lhs, const ff::list<T, SharedNodePool>& other)
{
    return lhs.size() == other.size() && std::equal(lhs.cbegin(), lhs.cend(), other.cbegin(), other.cend());
}

template<class T, bool SharedNodePool>
bool operator!=(const ff::list<T, SharedNodePool>& lhs, const ff::list<T, SharedNodePool>& other)
{
    return !(lhs == other);
}

template<class T, bool SharedNodePool>
bool operator<(const ff::list<T, SharedNodePool>& lhs, const ff::list<T, SharedNodePool>& other)
{
    return std::lexicographical_compare(lhs.cbegin(), lhs.cend(), other.cbegin(), other.cend());
}

template<class T, bool SharedNodePool>
bool operator<=(const ff::list<T, SharedNodePool>& lhs, const ff::list<T, SharedNodePool>& other)
{
    return !(other < lhs);
}

template<class T, bool SharedNodePool>
bool operator>(const ff::list<T, SharedNodePool>& lhs, const ff::list<T, SharedNodePool>& other)
{
    return other < lhs;
}

template<class T, bool SharedNodePool>
bool operator>=(const ff::list<T, SharedNodePool>& lhs, const ff::list<T, SharedNodePool>& other)
{
    return !(lhs < other);
}

namespace std
{
    template<class T, bool SharedNodePool>
    void swap(ff::list<T, SharedNodePool>& lhs, ff::list<T, SharedNodePool>& other) noexcept(noexcept(lhs.swap(other)))
    {
        lhs.swap(other);
    }
}
