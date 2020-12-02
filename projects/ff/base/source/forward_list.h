#pragma once

#include "pool_allocator.h"
#include "type_helper.h"

namespace ff::internal
{
    template<class T>
    struct forward_list_node
    {
        using this_type = typename forward_list_node<T>;

        template<class... Args>
        forward_list_node(this_type* next, Args&&... args)
            : value(std::forward<Args>(args)...)
            , next(next)
        {}

        static this_type* get_before_begin(this_type*& head_node)
        {
            uint8_t* before_begin = reinterpret_cast<uint8_t*>(&head_node) - offsetof(this_type, next);
            return reinterpret_cast<this_type*>(before_begin);
        }

        T value;
        this_type* next;
    };

    template<class T, bool SharedNodePool>
    class forward_list_node_pool
    {
    protected:
        using node_pool_type = typename ff::pool_allocator<ff::internal::forward_list_node<T>, false>;

        node_pool_type& get_node_pool()
        {
            return this->node_pool;
        }

    private:
        node_pool_type node_pool;
    };

    template<class T>
    class forward_list_node_pool<T, true>
    {
    protected:
        using node_pool_type = typename ff::pool_allocator<ff::internal::forward_list_node<T>, false>;

        static node_pool_type& get_node_pool()
        {
            static node_pool_type node_pool;
            return node_pool;
        }
    };

    template<class T>
    class forward_list_iterator
    {
    public:
        using this_type = typename forward_list_iterator<T>;
        using node_type = typename forward_list_node<typename std::remove_const_t<T>>;
        using iterator_category = typename std::forward_iterator_tag;
        using value_type = typename T;
        using difference_type = typename ptrdiff_t;
        using pointer = typename T*;
        using reference = typename T&;

        forward_list_iterator(node_type* node)
            : node(node)
        {}

        forward_list_iterator(const this_type& other)
            : node(other.node)
        {}

        // Convert non-const iterator to const iterator
        template<typename = std::enable_if_t<std::is_const_v<T>>>
        forward_list_iterator(const forward_list_iterator<std::remove_const_t<T>>& other)
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

        bool operator==(const this_type& other) const
        {
            return this->node == other.node;
        }

        bool operator!=(const this_type& other) const
        {
            return this->node != other.node;
        }

    private:
        node_type* node;
    };
}

namespace ff
{
    /// <summary>
    /// Replacement class for std::forward_list
    /// </summary>
    /// <remarks>
    /// This is a replacement for the std::forward_list class, but it always uses a memory pool for items.
    /// </remarks>
    /// <typeparam name="T">Item type</typeparam>
    /// <typeparam name="SharedNodePool">True to share the node pool among all lists of same type</typeparam>
    template<class T, bool SharedNodePool = false>
    class forward_list : private ff::internal::forward_list_node_pool<T, SharedNodePool>
    {
    public:
        using this_type = typename forward_list<T, SharedNodePool>;
        using node_type = typename ff::internal::forward_list_node<T>;
        using value_type = typename T;
        using size_type = typename size_t;
        using difference_type = typename ptrdiff_t;
        using reference = typename value_type&;
        using const_reference = typename const value_type&;
        using pointer = typename T*;
        using const_pointer = typename const T*;
        using iterator = typename ff::internal::forward_list_iterator<T>;
        using const_iterator = typename ff::internal::forward_list_iterator<const T>;

        explicit forward_list()
            : head_node(nullptr)
        {}

        forward_list(size_type count, const T& value)
            : forward_list()
        {
            this->resize(count, value);
        }

        explicit forward_list(size_type count)
            : forward_list()
        {
            this->resize(count);
        }

        template<class InputIt, std::enable_if_t<ff::internal::is_iterator_t<InputIt>, int> = 0>
        forward_list(InputIt first, InputIt last)
            : forward_list()
        {
            this->assign(first, last);
        }

        forward_list(const this_type& other)
            : forward_list()
        {
            *this = other;
        }

        forward_list(this_type&& other)
            : forward_list()
        {
            *this = std::move(other);
        }

        forward_list(std::initializer_list<T> init)
            : forward_list()
        {
            this->assign(init);
        }

        ~forward_list()
        {
            this->clear();
        }

        forward_list& operator=(const this_type& other)
        {
            if (this != &other)
            {
                this->assign(other.cbegin(), other.cend());
            }

            return *this;
        }

        forward_list& operator=(this_type&& other) noexcept
        {
            if (this != &other)
            {
                this->clear();
                this->get_node_pool() = std::move(other.get_node_pool());
                std::swap(this->head_node, other.head_node);
            }

            return *this;
        }

        forward_list& operator=(std::initializer_list<T> ilist)
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
            this->insert_after(this->cbefore_begin(), first, last);
        }

        void assign(std::initializer_list<T> ilist)
        {
            this->clear();
            this->insert_after(this->cbefore_begin(), ilist);
        }

        // allocator_type get_allocator() const noexcept

        reference front()
        {
            assert(!this->empty());
            return *this->begin();
        }

        const_reference front() const
        {
            assert(!this->empty());
            return *this->cbegin();
        }

        iterator before_begin() noexcept
        {
            return iterator(node_type::get_before_begin(this->head_node));
        }

        const_iterator before_begin() const noexcept
        {
            return this->cbefore_begin();
        }

        const_iterator cbefore_begin() const noexcept
        {
            node_type*& head_node = const_cast<node_type*&>(this->head_node);
            node_type* before_begin_node = node_type::get_before_begin(head_node);
            return const_iterator(before_begin_node);
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

        bool empty() const noexcept
        {
            return this->head_node == nullptr;
        }

        size_type max_size() const noexcept
        {
            return std::numeric_limits<size_type>::max();
        }

        void clear() noexcept
        {
            this->erase_after(this->cbefore_begin(), this->cend());
        }

        void resize(size_type count)
        {
            const_iterator prev = this->cbefore_begin();
            for (const_iterator cur = this->cbegin(); count && cur != this->cend(); prev = cur++, count--);
            this->erase_after(prev, this->cend());

            for (; count; count--)
            {
                prev = this->emplace_after(prev);
            }
        }

        void resize(size_type count, const value_type& value)
        {
            const_iterator prev = this->cbefore_begin();
            for (const_iterator cur = this->cbegin(); count && cur != this->cend(); prev = cur++, count--);
            this->erase_after(prev, this->cend());
            this->insert_after(prev, count, value);
        }

        iterator insert_after(const_iterator pos, const T& value)
        {
            return this->emplace_after(pos, value);
        }

        iterator insert_after(const_iterator pos, T&& value)
        {
            return this->emplace_after(pos, std::move(value));
        }

        iterator insert_after(const_iterator pos, size_type count, const T& value)
        {
            for (; count; count--)
            {
                pos = this->insert_after(pos, value);
            }

            return iterator(pos.internal_node());
        }

        template<class InputIt, std::enable_if_t<ff::internal::is_iterator_t<InputIt>, int> = 0>
        iterator insert_after(const_iterator pos, InputIt first, InputIt last)
        {
            for (; first != last; ++first)
            {
                pos = this->insert_after(pos, *first);
            }

            return iterator(pos.internal_node());
        }

        iterator insert_after(const_iterator pos, std::initializer_list<T> ilist)
        {
            return this->insert_after(pos, ilist.begin(), ilist.end());
        }

        template<class... Args>
        iterator emplace_after(const_iterator pos, Args&&... args)
        {
            node_type* node = this->get_node_pool().new_obj(pos.internal_node()->next, std::forward<Args>(args)...);
            pos.internal_node()->next = node;
            return iterator(node);
        }

        template<class... Args>
        reference emplace_front(Args&&... args)
        {
            return *this->emplace_after(this->cbefore_begin(), std::forward<Args>(args)...);
        }

        iterator erase_after(const_iterator pos)
        {
            const_iterator after = pos++;
            if (pos.internal_node())
            {
                pos = this->erase_after(after, ++pos);
            }

            return iterator(pos.internal_node());
        }

        iterator erase_after(const_iterator first, const_iterator last)
        {
            assert(first != last);
            node_type* node = first.internal_node()->next;
            first.internal_node()->next = last.internal_node();

            while (node != last.internal_node())
            {
                node_type* next = node->next;
                this->get_node_pool().delete_obj(node);
                node = next;
            }

            return iterator(node);
        }

        void push_front(const T& value)
        {
            this->emplace_front(value);
        }

        void push_front(T&& value)
        {
            this->emplace_front(std::move(value));
        }

        void pop_front()
        {
            assert(!this->empty());
            this->erase_after(this->cbefore_begin());
        }

        void swap(this_type& other) noexcept
        {
            if (this != &other)
            {
                std::swap(this->get_node_pool(), other.get_node_pool());
                std::swap(this->head_node, other.head_node);
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

        void splice_after(const_iterator pos, this_type& other)
        {
            this->splice_after(pos, std::move(other), other.cbefore_begin(), other.cend());
        }

        void splice_after(const_iterator pos, this_type&& other)
        {
            this->splice_after(pos, std::move(other), other.cbefore_begin(), other.cend());
        }

        void splice_after(const_iterator pos, this_type& other, const_iterator it)
        {
            this->splice_after(pos, std::move(other), it, other.cend());
        }

        void splice_after(const_iterator pos, this_type&& other, const_iterator it)
        {
            this->splice_after(pos, std::move(other), it, other.cend());
        }

        void splice_after(const_iterator pos, this_type& other, const_iterator first, const_iterator last)
        {
            this->splice_after(pos, std::move(other), first, last);
        }

        void splice_after(const_iterator pos, this_type&& other, const_iterator first, const_iterator last)
        {
            if (&this->get_node_pool() == &other.get_node_pool())
            {
                const_iterator after = first;
                if (first != last && ++after != last)
                {
                    const_iterator prev_last = first;
                    for (; after != last; prev_last = after++);

                    node_type* extracted_head = first.internal_node()->next;
                    first.internal_node()->next = after.internal_node();
                    prev_last.internal_node()->next = pos.internal_node()->next;
                    pos.internal_node()->next = extracted_head;
                }
            }
            else // Not possible to really splice, since the node pools are different. Just insert/delete instead.
            {
                const_iterator insert_after = pos;
                for (iterator cur = first.internal_node(), last2 = last.internal_node(); ;)
                {
                    if (++cur != last2)
                    {
                        insert_after = this->emplace_after(insert_after, std::move(*cur));
                    }
                    else
                    {
                        break;
                    }
                }

                other.erase_after(first, last);
            }
        }

        size_type remove(const T& value)
        {
            size_type removed = 0;
            const_iterator prev = this->cbefore_begin();
            for (const_iterator cur = this->cbegin(); cur != this->cend(); prev = cur++)
            {
                if (value == *cur)
                {
                    this->erase_after(prev);
                    cur = prev;
                    removed++;
                }
            }

            return removed;
        }

        template<class UnaryPredicate>
        size_type remove_if(UnaryPredicate p)
        {
            size_type removed = 0;
            const_iterator prev = this->cbefore_begin();
            for (const_iterator cur = this->cbegin(); cur != this->cend(); prev = cur++)
            {
                if (p(*cur))
                {
                    this->erase_after(prev);
                    cur = prev;
                    removed++;
                }
            }

            return removed;
        }

        void reverse() noexcept
        {
            node_type* cur = this->head_node;
            for (node_type* prev = nullptr, *next; cur; prev = cur, cur = next)
            {
                next = cur->next;
                cur->next = prev;
                if (!next)
                {
                    this->head_node = cur;
                    break;
                }
            }
        }

    private:
        node_type* head_node;
    };
}

template<class T, bool SharedNodePool>
bool operator==(const ff::forward_list<T, SharedNodePool>& lhs, const ff::forward_list<T, SharedNodePool>& other)
{
    return std::equal(lhs.cbegin(), lhs.cend(), other.cbegin(), other.cend());
}

template<class T, bool SharedNodePool>
bool operator!=(const ff::forward_list<T, SharedNodePool>& lhs, const ff::forward_list<T, SharedNodePool>& other)
{
    return !(lhs == other);
}

template<class T, bool SharedNodePool>
bool operator<(const ff::forward_list<T, SharedNodePool>& lhs, const ff::forward_list<T, SharedNodePool>& other)
{
    return std::lexicographical_compare(lhs.cbegin(), lhs.cend(), other.cbegin(), other.cend());
}

template<class T, bool SharedNodePool>
bool operator<=(const ff::forward_list<T, SharedNodePool>& lhs, const ff::forward_list<T, SharedNodePool>& other)
{
    return !(other < lhs);
}

template<class T, bool SharedNodePool>
bool operator>(const ff::forward_list<T, SharedNodePool>& lhs, const ff::forward_list<T, SharedNodePool>& other)
{
    return other < lhs;
}

template<class T, bool SharedNodePool>
bool operator>=(const ff::forward_list<T, SharedNodePool>& lhs, const ff::forward_list<T, SharedNodePool>& other)
{
    return !(lhs < other);
}

namespace std
{
    template<class T, bool SharedNodePool>
    void swap(ff::forward_list<T, SharedNodePool>& lhs, ff::forward_list<T, SharedNodePool>& other) noexcept(noexcept(lhs.swap(other)))
    {
        lhs.swap(other);
    }
}
