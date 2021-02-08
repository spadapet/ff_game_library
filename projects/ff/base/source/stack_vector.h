#pragma once

#include "type_helper.h"

namespace ff::internal
{
    template<class T, size_t StackSize, class Enabled = void>
    class vector_stack_storage;

    template<class T, size_t StackSize>
    class alignas(T) vector_stack_storage<T, StackSize, std::enable_if_t<StackSize != 0>>
    {
    protected:
        T* get_stack_items()
        {
            return reinterpret_cast<T*>(this->item_stack);
        }

    private:
        alignas(T) uint8_t item_stack[sizeof(T) * StackSize];
    };

    template<class T, size_t StackSize>
    class vector_stack_storage<T, StackSize, std::enable_if_t<StackSize == 0>>
    {
    protected:
        constexpr T* get_stack_items()
        {
            return nullptr;
        }
    };

    template<class T>
    class vector_allocator
    {
    protected:
        static void deallocate(T* const ptr, const size_t count)
        {
            ::_aligned_free(ptr);
        }

        static T* allocate(const size_t count, size_t& count_allocated)
        {
            count_allocated = ff::math::nearest_power_of_two(std::max<size_t>(count, 16));
            return reinterpret_cast<T*>(::_aligned_malloc(sizeof(T) * count_allocated, alignof(T)));
        }
    };
}

namespace ff
{
    /// <summary>
    /// Replacement class for std::vector
    /// </summary>
    /// <remarks>
    /// This is a replacement for the std::vector class, but it fills stack storage before memory is allocated.
    /// </remarks>
    /// <typeparam name="T">Item type</typeparam>
    /// <typeparam name="StackSize">Initial buffer size on the stack</typeparam>
    template<class T, size_t StackSize = 0>
    class stack_vector
        : private ff::internal::vector_stack_storage<T, StackSize>
        , private ff::internal::vector_allocator<T>
        , private ff::internal::type_helper
    {
    public:
        using this_type = typename stack_vector<T, StackSize>;
        using value_type = typename T;
        using reference = typename T&;
        using const_reference = typename const T&;
        using allocator_type = typename ff::internal::vector_allocator<T>;
        using pointer = typename T*;
        using const_pointer = typename const T*;
        using size_type = typename size_t;
        using difference_type = typename ptrdiff_t;
        using iterator = typename pointer;
        using const_iterator = typename const_pointer;
        using reverse_iterator = typename std::reverse_iterator<iterator>;
        using const_reverse_iterator = typename std::reverse_iterator<const_iterator>;

        stack_vector() noexcept
            : data_(this->get_stack_items())
            , capacity_(StackSize)
            , size_(0)
        {}

        stack_vector(size_type count, const T& value)
            : stack_vector()
        {
            this->assign(count, value);
        }

        explicit stack_vector(size_type count)
            : stack_vector()
        {
            this->resize(count);
        }

        template<class InputIt, std::enable_if_t<ff::internal::is_iterator_t<InputIt>, int> = 0>
        stack_vector(InputIt first, InputIt last)
            : stack_vector()
        {
            this->assign(first, last);
        }

        stack_vector(const this_type& other)
            : stack_vector()
        {
            this->assign(other.cbegin(), other.cend());
        }

        stack_vector(this_type&& other) noexcept
            : stack_vector()
        {
            *this = std::move(other);
        }

        stack_vector(std::initializer_list<T> init)
            : stack_vector()
        {
            this->assign(init);
        }

        ~stack_vector()
        {
            this->clear();
            this->deallocate_item_data();
        }

        this_type& operator=(const this_type& other)
        {
            if (this != &other)
            {
                this->assign(other.cbegin(), other.cend());
            }

            return *this;
        }

        this_type& operator=(this_type&& other) noexcept
        {
            if (this != &other)
            {
                this->clear();
                this->deallocate_item_data();

                if (other.data_ == other.get_stack_items())
                {
                    ff::internal::type_helper::shift_items(this->data_, other.data_, other.size_);
                }
                else
                {
                    this->data_ = other.data_;
                }

                this->capacity_ = other.capacity_;
                this->size_ = other.size_;

                other.data_ = other.get_stack_items();
                other.capacity_ = StackSize;
                other.size_ = 0;
            }

            return *this;
        }

        this_type& operator=(std::initializer_list<T> ilist)
        {
            this->assign(ilist);
            return *this;
        }

        void assign(size_type count, const T& value)
        {
            this->clear();
            this->insert(this->cbegin(), count, value);
        }

        template<class InputIt, std::enable_if_t<ff::internal::is_iterator_t<InputIt>, int> = 0>
        void assign(InputIt first, InputIt last)
        {
            this->clear();
            this->insert(this->cbegin(), first, last);
        }

        void assign(std::initializer_list<T> ilist)
        {
            this->clear();
            this->insert(this->cbegin(), ilist);
        }

        allocator_type get_allocator() const noexcept
        {
            return allocator_type();
        }

        const_reference at(size_type pos) const
        {
            assert(pos < this->size_);
            return this->data_[pos];
        }

        reference at(size_type pos)
        {
            assert(pos < this->size_);
            return this->data_[pos];
        }

        reference operator[](size_type pos)
        {
            return this->at(pos);
        }

        const_reference operator[](size_type pos) const
        {
            return this->at(pos);
        }

        reference front()
        {
            assert(this->size_);
            return this->data_[0];
        }

        const_reference front() const
        {
            assert(this->size_);
            return this->data_[0];
        }

        reference back()
        {
            assert(this->size_);
            return this->data_[this->size_ - 1];
        }

        const_reference back() const
        {
            assert(this->size_);
            return this->data_[this->size_ - 1];
        }

        T* data() noexcept
        {
            return this->data_;
        }

        const T* data() const noexcept
        {
            return this->data_;
        }

        iterator begin() noexcept
        {
            return iterator(this->data_);
        }

        const_iterator begin() const noexcept
        {
            return const_iterator(this->data_);
        }

        const_iterator cbegin() const noexcept
        {
            return const_iterator(this->data_);
        }

        iterator end() noexcept
        {
            return iterator(this->data_ + this->size_);
        }

        const_iterator end() const noexcept
        {
            return const_iterator(this->data_ + this->size_);
        }

        const_iterator cend() const noexcept
        {
            return const_iterator(this->data_ + this->size_);
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
            return this->size_ == 0;
        }

        size_type size() const noexcept
        {
            return this->size_;
        }

        size_type byte_size() const noexcept
        {
            return this->size_ * sizeof(T);
        }

        size_type max_size() const noexcept
        {
            return std::numeric_limits<size_type>::max();
        }

        void reserve(size_type new_cap)
        {
            this->reserve_item_data(this->cbegin(), 0, new_cap);
        }

        size_type capacity() const noexcept
        {
            return this->capacity_;
        }

        void shrink_to_fit()
        {
            if (this->size_ < this->capacity_ && this->data_ != this->get_stack_items())
            {
                if (this->size_ == 0)
                {
                    this->deallocate_item_data();
                }
                else
                {
                    size_type new_size = this->size_;
                    size_type new_cap = StackSize;
                    T* new_data = (new_size <= new_cap) ? this->get_stack_items() : this->allocate_item_data(this->size_, new_cap);
                    ff::internal::type_helper::shift_items(new_data, this->data_, this->size_);
                    this->deallocate_item_data();

                    this->data_ = new_data;
                    this->capacity_ = new_cap;
                    this->size_ = new_size;
                }
            }
        }

        void clear() noexcept
        {
            this->erase(this->cbegin(), this->cend());
        }

        iterator insert(const_iterator pos, const T& value)
        {
            iterator iter = this->reserve_item_data(pos, 1);
            ff::internal::type_helper::copy_construct_item(&*iter, value);
            return iter;
        }

        iterator insert(const_iterator pos, T&& value)
        {
            iterator iter = this->reserve_item_data(pos, 1);
            ff::internal::type_helper::move_construct_item(&*iter, std::move(value));
            return iter;
        }

        iterator insert(const_iterator pos, size_type count, const T& value)
        {
            iterator iter = this->reserve_item_data(pos, count);
            for (size_type i = 0; i < count; i++)
            {
                ff::internal::type_helper::copy_construct_item(&iter[i], value);
            }

            return iter;
        }

        template<class InputIt, std::enable_if_t<ff::internal::is_iterator_t<InputIt>, int> = 0>
        iterator insert(const_iterator pos, InputIt first, InputIt last)
        {
            return this->insert_range(pos, first, last, typename std::iterator_traits<InputIt>::iterator_category{});
        }

        iterator insert(const_iterator pos, std::initializer_list<T> ilist)
        {
            return this->insert(pos, ilist.begin(), ilist.end());
        }

        template<class... Args>
        iterator emplace(const_iterator pos, Args&&... args)
        {
            iterator iter = this->reserve_item_data(pos, 1);
            ff::internal::type_helper::construct_item(&*iter, std::forward<Args>(args)...);
            return iter;
        }

        iterator erase(const_iterator pos)
        {
            return this->erase(pos, pos + 1);
        }

        iterator erase(const_iterator first, const_iterator last)
        {
            assert(first >= this->cbegin() && last >= first && last <= this->cend());

            size_type i = first - this->cbegin();
            size_type count = last - first;
            ff::internal::type_helper::destruct_items(this->data_ + i, count);
            ff::internal::type_helper::shift_items(this->data_ + i, this->data_ + i + count, this->size_ - i - count);
            this->size_ -= count;

            return iterator(this->data_ + i);
        }

        void push_back(const T& value)
        {
            this->insert(this->cend(), value);
        }

        void push_back(T&& value)
        {
            this->insert(this->cend(), std::move(value));
        }

        template<class... Args>
        reference emplace_back(Args&&... args)
        {
            return *this->emplace(this->cend(), std::forward<Args>(args)...);
        }

        void pop_back()
        {
            assert(this->size_);
            this->erase(this->cend() - 1, this->cend());
        }

        void resize(size_type count)
        {
            if (count < this->size_)
            {
                this->erase(this->cend() - count, this->cend());
            }
            else if (count > this->size_)
            {
                size_type new_count = count - this->size_;
                iterator i = this->reserve_item_data(this->cend(), new_count);
                ff::internal::type_helper::default_construct_items(&*i, new_count);
            }
        }

        void resize(size_type count, const value_type& value)
        {
            if (count < this->size_)
            {
                this->erase(this->cend() - count, this->cend());
            }
            else if (count > this->size_)
            {
                this->insert(this->cend(), count, value);
            }
        }

        void swap(this_type& other)
        {
            this_type temp = std::move(other);
            other = std::move(*this);
            *this = std::move(temp);
        }

    private:
        template<class InputIt>
        iterator insert_range(const_iterator pos, InputIt first, InputIt last, std::input_iterator_tag)
        {
            this_type temp;
            for (; first != last; ++first)
            {
                temp.push_back(*first);
            }

            iterator iter = this->reserve_item_data(pos, temp.size());
            for (size_type i = 0; i < temp.size(); i++)
            {
                ff::internal::type_helper::move_construct_item(&iter[i], std::move(temp[i]));
            }

            return iter;
        }

        template<class InputIt>
        iterator insert_range(const_iterator pos, InputIt first, InputIt last, std::forward_iterator_tag)
        {
            iterator iter = this->reserve_item_data(pos, std::distance(first, last));
            for (; first != last; ++first, ++iter)
            {
                ff::internal::type_helper::copy_construct_item(&*iter, *first);
            }

            return iter;
        }

        iterator reserve_item_data(const_iterator pos, size_type count, size_type new_cap = 0)
        {
            difference_type pos_index = pos - this->cbegin();
            assert(pos_index >= 0 && static_cast<size_type>(pos_index) <= this->size_);

            size_type new_size = this->size_ + count;
            new_cap = std::max(new_cap, this->size_ + count);

            if (new_cap > this->capacity_)
            {
                T* new_data = this->allocate_item_data(new_cap, new_cap);
                ff::internal::type_helper::shift_items(new_data, this->data_, pos_index);
                ff::internal::type_helper::shift_items(new_data + pos_index + count, this->data_ + pos_index, this->size_ - pos_index);
                this->deallocate_item_data();

                this->data_ = new_data;
                this->capacity_ = new_cap;
            }
            else if (static_cast<size_type>(pos_index) < this->size_)
            {
                ff::internal::type_helper::shift_items(this->data_ + pos_index + count, this->data_ + pos_index, this->size_ - pos_index);
            }

            this->size_ = new_size;

            return iterator(this->data_ + pos_index);
        }

        T* allocate_item_data(size_type capacity_requested, size_type& capacity)
        {
            return allocator_type::allocate(capacity_requested, capacity);
        }

        void deallocate_item_data()
        {
            if (this->data_ != this->get_stack_items())
            {
                allocator_type::deallocate(this->data_, this->capacity_);
                this->data_ = this->get_stack_items();
            }

            this->capacity_ = StackSize;
            this->size_ = 0;
        }

        T* data_;
        size_type capacity_;
        size_type size_;
    };

    template<class T, size_t StackSize>
    size_t vector_byte_size(const ff::stack_vector<T, StackSize>& vec)
    {
        return vec.size() * sizeof(typename ff::stack_vector<T, StackSize>::value_type);
    }

    template<class T, class Alloc>
    size_t vector_byte_size(const std::vector<T, Alloc>& vec)
    {
        return vec.size() * sizeof(typename std::vector<T, Alloc>::value_type);
    }

    template<class T, size_t Size>
    size_t array_byte_size(const std::array<T, Size>& arr)
    {
        return Size * sizeof(T);
    }
}

template<class T, size_t StackSize>
bool operator==(const ff::stack_vector<T, StackSize>& lhs, const ff::stack_vector<T, StackSize>& rhs)
{
    return lhs.size() == rhs.size() && std::equal(lhs.cbegin(), lhs.cend(), rhs.cbegin(), rhs.cend());
}

template<class T, size_t StackSize>
bool operator!=(const ff::stack_vector<T, StackSize>& lhs, const ff::stack_vector<T, StackSize>& rhs)
{
    return !(lhs == rhs);
}

template<class T, size_t StackSize>
bool operator<(const ff::stack_vector<T, StackSize>& lhs, const ff::stack_vector<T, StackSize>& rhs)
{
    return std::lexicographical_compare(lhs.cbegin(), lhs.cend(), rhs.cbegin(), rhs.cend());
}

template<class T, size_t StackSize>
bool operator<=(const ff::stack_vector<T, StackSize>& lhs, const ff::stack_vector<T, StackSize>& rhs)
{
    return !(rhs < lhs);
}

template<class T, size_t StackSize>
bool operator>(const ff::stack_vector<T, StackSize>& lhs, const ff::stack_vector<T, StackSize>& rhs)
{
    return rhs < lhs;
}

template<class T, size_t StackSize>
bool operator>=(const ff::stack_vector<T, StackSize>& lhs, const ff::stack_vector<T, StackSize>& rhs)
{
    return !(lhs < rhs);
}

namespace std
{
    template<class T, size_t StackSize>
    void swap(ff::stack_vector<T, StackSize>& lhs, ff::stack_vector<T, StackSize>& rhs) noexcept(noexcept(lhs.swap(rhs)))
    {
        lhs.swap(rhs);
    }
}
