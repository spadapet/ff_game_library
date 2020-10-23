#pragma once
#include "pool_allocator.h"
#include "vector.h"

namespace ff
{
    /// <summary>
    /// </summary>
    /// <remarks>
    /// </remarks>
    /// <typeparam name="CharT">Character type</typeparam>
    template<class CharT>
    class basic_const_string
    {
    public:
#if 0
        using string_type = basic_string<CharT, StackSize, Traits, Allocator>;
        using vector_type = ff::vector<CharT, StackSize, Allocator>;
        using allocator_type = typename vector_type::allocator_type;
        using value_type = typename vector_type::value_type;
        using reference = typename vector_type::reference;
        using const_reference = typename vector_type::const_reference;
        using pointer = typename vector_type::pointer;
        using const_pointer = typename vector_type::const_pointer;
        using size_type = typename vector_type::size_type;
        using difference_type = typename vector_type::difference_type;
        using iterator = typename vector_type::iterator;
        using const_iterator = typename vector_type::const_iterator;
        using reverse_iterator = typename vector_type::reverse_iterator;
        using const_reverse_iterator = typename vector_type::const_reverse_iterator;

        static const size_type npos = -1;

        explicit basic_string(const Allocator& alloc) noexcept
            : string_data_ref(&string_type::get_empty_string_data())
        {
        }

        basic_string() noexcept(noexcept(Allocator()))
            : basic_string(Allocator())
        {
        }

        basic_string(size_type count, CharT ch, const Allocator& alloc = Allocator())
            : basic_string(alloc)
        {
        }

        basic_string(const string_type& other, size_type pos, const Allocator& alloc = Allocator())
            : basic_string(alloc)
        {
        }

        basic_string(const string_type& other, size_type pos, size_type count, const Allocator& alloc = Allocator())
            : basic_string(alloc)
        {
        }

        basic_string(const CharT* s, size_type count, const Allocator& alloc = Allocator())
            : basic_string(alloc)
        {
        }

        basic_string(const CharT* s, const Allocator& alloc = Allocator())
            : basic_string(alloc)
        {
        }

        template<class InputIt>
        basic_string(InputIt first, InputIt last, const Allocator& alloc = Allocator())
            : basic_string(alloc)
        {
        }

        basic_string(const string_type& other)
            : basic_string(other.get_allocator())
        {
        }

        basic_string(const string_type& other, const Allocator& alloc)
            : basic_string(alloc)
        {
        }

        basic_string(string_type&& other) noexcept
            : basic_string(other.get_allocator())
        {
        }

        basic_string(string_type&& other, const Allocator& alloc)
            : basic_string(alloc)
        {
        }

        basic_string(std::initializer_list<CharT> ilist, const Allocator& alloc = Allocator())
            : basic_string(alloc)
        {
        }

        template<class T>
        explicit basic_string(const T& t, const Allocator& alloc = Allocator())
            : basic_string(alloc)
        {
        }

        template<class T>
        basic_string(const T& t, size_type pos, size_type n, const Allocator& alloc = Allocator())
            : basic_string(alloc)
        {
        }

        ~basic_string()
        {
            this->release_string_data();
        }

        string_type& operator=(const string_type& str);
        string_type& operator=(string_type&& str) noexcept(std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value || std::allocator_traits<Allocator>::is_always_equal::value);
        string_type& operator=(const CharT* s);
        string_type& operator=(CharT ch);
        string_type& operator=(std::initializer_list<CharT> ilist);
        template<class T>
        string_type& operator=(const T& t);

        string_type& assign(size_type count, CharT ch);
        string_type& assign(const string_type& str);
        string_type& assign(const string_type& str, size_type pos, size_type count = string_type::npos);
        string_type& assign(string_type&& str) noexcept(std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value || std::allocator_traits<Allocator>::is_always_equal::value);
        string_type& assign(const CharT* s, size_type count);
        string_type& assign(const CharT* s);
        template<class InputIt>
        string_type& assign(InputIt first, InputIt last);
        string_type& assign(std::initializer_list<CharT> ilist);
        template<class T>
        string_type& assign(const T& t);
        template<class T>
        string_type& assign(const T& t, size_type pos, size_type count = string_type::npos);

        allocator_type get_allocator() const
        {
            return Allocator{};
        }

        reference at(size_type pos);
        const_reference at(size_type pos) const;
        reference operator[](size_type pos);
        const_reference operator[](size_type pos) const;
        CharT& front();
        const CharT& front() const;
        CharT& back();
        const CharT& back() const;

        const CharT* data() const noexcept
        {
            return this->get_const_char_vector().data();
        }

        CharT* data() noexcept
        {
            return this->get_editable_char_vector().data();
        }

        const CharT* c_str() const noexcept
        {
            return this->data();
        }

        operator std::basic_string_view<CharT, Traits>() const noexcept;

        iterator begin() noexcept
        {
            return this->get_editable_char_vector().begin();
        }

        const_iterator begin() const noexcept
        {
            return this->get_const_char_vector().begin();
        }

        const_iterator cbegin() const noexcept
        {
            return this->get_const_char_vector().cbegin();
        }

        iterator end() noexcept
        {
            return this->get_editable_char_vector().end() - 1;
        }

        const_iterator end() const noexcept
        {
            return this->get_const_char_vector().end() - 1;
        }

        const_iterator cend() const noexcept
        {
            return this->get_const_char_vector().end() - 1;
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
            return this->size() == 0;
        }

        size_type size() const noexcept
        {
            return this->get_const_char_vector().size() - 1;
        }

        size_type length() const noexcept
        {
            return this->size();
        }

        size_type max_size() const noexcept
        {
            return std::numeric_limits<size_t>::max();
        }

        void reserve(size_type new_cap = 0)
        {
            if (new_cap == 0)
            {
                this->shrink_to_fit();
            }
            else
            {
                this->get_editable_char_vector().reserve(new_cap + 1);
            }
        }

        size_type capacity() const noexcept
        {
            return this->get_const_char_vector().capacity() - 1;
        }

        void shrink_to_fit()
        {
            this->get_editable_char_vector().shrink_to_fit();
        }

        void clear() noexcept
        {
            this->erase(this->cbegin(), this->cend());
        }

        string_type& insert(size_type index, size_type count, CharT ch);
        string_type& insert(size_type index, const CharT* s);
        string_type& insert(size_type index, const CharT* s, size_type count);
        string_type& insert(size_type index, const string_type& str);
        string_type& insert(size_type index, const string_type& str, size_type index_str, size_type count = string_type::npos);

        iterator insert(const_iterator pos, CharT ch)
        {
            return this->get_editable_char_vector(pos).insert(pos, ch);
        }

        iterator insert(const_iterator pos, size_type count, CharT ch)
        {
            return this->get_editable_char_vector(pos).insert(pos, count, ch);
        }

        template<class InputIt>
        iterator insert(const_iterator pos, InputIt first, InputIt last)
        {
            return this->get_editable_char_vector(pos).insert(pos, first, last);
        }

        iterator insert(const_iterator pos, std::initializer_list<CharT> ilist)
        {
            return this->get_editable_char_vector(pos).insert(pos, ilist);
        }

        template<class T>
        string_type& insert(size_type pos, const T& t);

        template<class T>
        string_type& insert(size_type index, const T& t, size_type index_str, size_type count = string_type::npos);

        string_type& erase(size_type index = 0, size_type count = string_type::npos)
        {
            return this->erase(this->cbegin() + index, (count == string_type::npos) ? this->cend() : this->cbegin() + index + count);
        }

        iterator erase(const_iterator position)
        {
            return this->get_editable_char_vector(position).erase(position);
        }

        iterator erase(const_iterator first, const_iterator last)
        {
            return this->get_editable_char_vector(first, last).erase(first, last);
        }

        void push_back(CharT ch)
        {
            this->insert(this->cend(), ch);
        }

        void pop_back()
        {
            assert(!this->empty());
            this->get_editable_char_vector().erase(this->cend() - 2);
        }

        string_type& append(size_type count, CharT ch);
        string_type& append(const string_type& str);
        string_type& append(const string_type& str, size_type pos, size_type count = string_type::npos);
        string_type& append(const CharT* s, size_type count);
        string_type& append(const CharT* s);
        template<class InputIt>
        string_type& append(InputIt first, InputIt last);
        string_type& append(std::initializer_list<CharT> ilist);
        template<class T>
        string_type& append(const T& t);
        template<class T>
        string_type& append(const T& t, size_type pos, size_type count = string_type::npos);

        string_type& operator+=(const string_type& str);
        string_type& operator+=(CharT ch);
        string_type& operator+=(const CharT* s);
        string_type& operator+=(std::initializer_list<CharT> ilist);
        template<class T>
        string_type& operator+=(const T& t);

        int compare(const string_type& str) const noexcept;
        int compare(size_type pos1, size_type count1, const string_type& str) const;
        int compare(size_type pos1, size_type count1, const string_type& str, size_type pos2, size_type count2 = string_type::npos) const;
        int compare(const CharT* s) const;
        int compare(size_type pos1, size_type count1, const CharT* s) const;
        int compare(size_type pos1, size_type count1, const CharT* s, size_type count2) const;
        template<class T>
        int compare(const T& t) const noexcept(std::is_nothrow_convertible_v<const T&, std::basic_string_view<CharT, Traits>>);
        template<class T>
        int compare(size_type pos1, size_type count1, const T& t) const;
        template<class T>
        int compare(size_type pos1, size_type count1, const T& t, size_type pos2, size_type count2 = string_type::npos) const;

        string_type& replace(size_type pos, size_type count, const string_type& str);
        string_type& replace(const_iterator first, const_iterator last, const string_type& str);
        string_type& replace(size_type pos, size_type count, const string_type& str, size_type pos2, size_type count2 = string_type::npos);
        template<class InputIt>
        string_type& replace(const_iterator first, const_iterator last, InputIt first2, InputIt last2);
        string_type& replace(size_type pos, size_type count, const CharT* cstr, size_type count2);
        string_type& replace(const_iterator first, const_iterator last, const CharT* cstr, size_type count2);
        string_type& replace(size_type pos, size_type count, const CharT* cstr);
        string_type& replace(const_iterator first, const_iterator last, const CharT* cstr);
        string_type& replace(size_type pos, size_type count, size_type count2, CharT ch);
        string_type& replace(const_iterator first, const_iterator last, size_type count2, CharT ch);
        string_type& replace(const_iterator first, const_iterator last, std::initializer_list<CharT> ilist);
        template<class T>
        string_type& replace(size_type pos, size_type count, const T& t);
        template<class T>
        string_type& replace(const_iterator first, const_iterator last, const T& t);
        template<class T>
        string_type& replace(size_type pos, size_type count, const T& t, size_type pos2, size_type count2 = string_type::npos);

        string_type substr(size_type pos = 0, size_type count = string_type::npos) const;
        size_type copy(CharT* dest, size_type count, size_type pos = 0) const;
        void resize(size_type count);
        void resize(size_type count, CharT ch);
        void swap(string_type& other) noexcept(std::allocator_traits<Allocator>::propagate_on_container_swap::value || std::allocator_traits<Allocator>::is_always_equal::value);

        size_type find(const string_type& str, size_type pos = 0) const noexcept;
        size_type find(const CharT* s, size_type pos, size_type count) const;
        size_type find(const CharT* s, size_type pos = 0) const;
        size_type find(CharT ch, size_type pos = 0) const noexcept;
        template<class T>
        size_type find(const T& t, size_type pos = 0) const noexcept(std::is_nothrow_convertible_v<const T&, std::basic_string_view<CharT, Traits>>);

        size_type rfind(const string_type& str, size_type pos = string_type::npos) const noexcept;
        size_type rfind(const CharT* s, size_type pos, size_type count) const;
        size_type rfind(const CharT* s, size_type pos = string_type::npos) const;
        size_type rfind(CharT ch, size_type pos = string_type::npos) const noexcept;
        template<class T>
        size_type rfind(const T& t, size_type pos = string_type::npos) const noexcept(std::is_nothrow_convertible_v<const T&, std::basic_string_view<CharT, Traits>>);

        size_type find_first_of(const string_type& str, size_type pos = 0) const noexcept;
        size_type find_first_of(const CharT* s, size_type pos, size_type count) const;
        size_type find_first_of(const CharT* s, size_type pos = 0) const;
        size_type find_first_of(CharT ch, size_type pos = 0) const noexcept;
        template<class T>
        size_type find_first_of(const T& t, size_type pos = 0) const noexcept(std::is_nothrow_convertible_v<const T&, std::basic_string_view<charT, traits>>);

        size_type find_first_not_of(const string_type& str, size_type pos = 0) const noexcept;
        size_type find_first_not_of(const CharT* s, size_type pos, size_type count) const;
        size_type find_first_not_of(const CharT* s, size_type pos = 0) const;
        size_type find_first_not_of(CharT ch, size_type pos = 0) const noexcept;
        template<class T>
        size_type find_first_not_of(const T& t, size_type pos = 0) const noexcept(std::is_nothrow_convertible_v<const T&, std::basic_string_view<charT, traits>>);

        size_type find_last_of(const string_type& str, size_type pos = string_type::npos) const noexcept;
        size_type find_last_of(const CharT* s, size_type pos, size_type count) const;
        size_type find_last_of(const CharT* s, size_type pos = string_type::npos) const;
        size_type find_last_of(CharT ch, size_type pos = string_type::npos) const noexcept;
        template<class T>
        size_type find_last_of(const T& t, size_type pos = string_type::npos) const noexcept(std::is_nothrow_convertible_v<const T&, std::basic_string_view<charT, traits>>);

        size_type find_last_not_of(const string_type& str, size_type pos = string_type::npos) const noexcept;
        size_type find_last_not_of(const CharT* s, size_type pos, size_type count) const;
        size_type find_last_not_of(const CharT* s, size_type pos = string_type::npos) const;
        size_type find_last_not_of(CharT ch, size_type pos = string_type::npos) const noexcept;
        template<class T>
        size_type find_last_not_of(const T& t, size_type pos = string_type::npos) const noexcept(std::is_nothrow_convertible_v<const T&, std::basic_string_view<charT, traits>>);

    private:
        struct string_data
        {
            std::atomic_int refs;
            vector_type char_vector;

            string_data(int refs, vector_type&& char_vector)
                : refs(refs)
                , char_vector(std::move(char_vector))
            {
            }
        };

        static string_data& get_empty_string_data()
        {
            static CharT empty_string[1]{ static_cast<CharT>(0) };
            static string_data empty_string_data{ 0, vector_type{ empty_string } };
            return empty_string_data;
        }

        vector_type& get_editable_char_vector()
        {
            if (this->string_data_ref == &string_type::get_empty_string_data() || this->string_data_ref->refs.load() > 1)
            {
                this->set_string_data(string_type::get_string_data_allocator().new_obj(0, vector_type{this->get_const_char_vector()}));
            }

            return this->string_data_ref->char_vector;
        }

        vector_type& get_editable_char_vector(const_iterator& pos)
        {
            difference_type index = pos - this->cbegin();
            vector_type& char_vector = this->get_editable_char_vector();
            pos = char_vector.cbegin() + index;

            return char_vector;
        }

        vector_type& get_editable_char_vector(const_iterator& pos1, const_iterator& pos2)
        {
            difference_type index1 = pos1 - this->cbegin();
            difference_type index2 = pos2 - this->cbegin();
            vector_type& char_vector = this->get_editable_char_vector();
            pos1 = char_vector.cbegin() + index1;
            pos2 = char_vector.cbegin() + index2;

            return char_vector;
        }

        const vector_type& get_const_char_vector() const
        {
            return this->string_data_ref->char_vector;
        }

        static ff::pool_allocator<string_data>& get_string_data_allocator()
        {
            static ff::pool_allocator<string_data> string_data_allocator;
            return string_data_allocator;
        }

        void set_string_data(string_data* new_string_data_ref)
        {
            if (this->string_data_ref != new_string_data_ref)
            {
                new_string_data_ref->refs.fetch_add(1);
                this->release_string_data();
                this->string_data_ref = new_string_data_ref;
            }
        }

        void release_string_data()
        {
            if (this->string_data_ref != &string_type::get_empty_string_data() && this->string_data_ref->refs.fetch_sub(1) == 1)
            {
                string_type::get_string_data_allocator().delete_obj(this->string_data_ref);
                this->string_data_ref = &string_type::get_empty_string_data();
            }
        }

        string_data* string_data_ref;
#endif
    };

    //extern template class ff::basic_string<char, 16>;
    //extern template class ff::basic_string<wchar_t, 8>;
    //extern template class ff::basic_string<char16_t, 8>;
    //extern template class ff::basic_string<char32_t, 8>;

    //using string = ff::basic_string<char, 16>;
    //using wstring = ff::basic_string<wchar_t, 8>;
    //using u16string = ff::basic_string<char16_t, 8>;
    //using u32string = ff::basic_string<char32_t, 8>;
}

#if 0
namespace std
{
    template<class CharT>
    void swap(ff::basic_const_string<CharT>& lhs, ff::basic_const_string<CharT>& rhs) noexcept(noexcept(lhs.swap(rhs)));

    template<>
    struct hash<ff::string>;

    template<>
    struct hash<ff::wstring>;

    template<>
    struct hash<ff::u16string>;

    template<>
    struct hash<ff::u32string>;
}
#endif

#if 0
template<class CharT, size_t StackSize, class Traits, class Alloc>
ff::basic_string<CharT, StackSize, Traits, Alloc> operator+(const ff::basic_string<CharT, StackSize, Traits, Alloc>& lhs, const ff::basic_string<CharT, StackSize, Traits, Alloc>& rhs);

template<class CharT, size_t StackSize, class Traits, class Alloc>
ff::basic_string<CharT, StackSize, Traits, Alloc> operator+(const ff::basic_string<CharT, StackSize, Traits, Alloc>& lhs, const CharT* rhs);

template<class CharT, size_t StackSize, class Traits, class Alloc>
ff::basic_string<CharT, StackSize, Traits, Alloc> operator+(const ff::basic_string<CharT, StackSize, Traits, Alloc>& lhs, CharT rhs);

template<class CharT, size_t StackSize, class Traits, class Alloc>
ff::basic_string<CharT, StackSize, Traits, Alloc> operator+(const CharT* lhs, const ff::basic_string<CharT, StackSize, Traits, Alloc>& rhs);

template<class CharT, size_t StackSize, class Traits, class Alloc>
ff::basic_string<CharT, StackSize, Traits, Alloc> operator+(CharT lhs, const ff::basic_string<CharT, StackSize, Traits, Alloc>& rhs);

template<class CharT, size_t StackSize, class Traits, class Alloc>
ff::basic_string<CharT, StackSize, Traits, Alloc> operator+(ff::basic_string<CharT, StackSize, Traits, Alloc>&& lhs, ff::basic_string<CharT, StackSize, Traits, Alloc>&& rhs);

template<class CharT, size_t StackSize, class Traits, class Alloc>
ff::basic_string<CharT, StackSize, Traits, Alloc> operator+(ff::basic_string<CharT, StackSize, Traits, Alloc>&& lhs, const ff::basic_string<CharT, StackSize, Traits, Alloc>& rhs);

template<class CharT, size_t StackSize, class Traits, class Alloc>
ff::basic_string<CharT, StackSize, Traits, Alloc> operator+(ff::basic_string<CharT, StackSize, Traits, Alloc>&& lhs, const CharT* rhs);

template<class CharT, size_t StackSize, class Traits, class Alloc>
ff::basic_string<CharT, StackSize, Traits, Alloc> operator+(ff::basic_string<CharT, StackSize, Traits, Alloc>&& lhs, CharT rhs);

template<class CharT, size_t StackSize, class Traits, class Alloc>
ff::basic_string<CharT, StackSize, Traits, Alloc> operator+(const ff::basic_string<CharT, StackSize, Traits, Alloc>& lhs, ff::basic_string<CharT, StackSize, Traits, Alloc>&& rhs);

template<class CharT, size_t StackSize, class Traits, class Alloc>
ff::basic_string<CharT, StackSize, Traits, Alloc> operator+(const CharT* lhs, ff::basic_string<CharT, StackSize, Traits, Alloc>&& rhs);

template<class CharT, size_t StackSize, class Traits, class Alloc>
ff::basic_string<CharT, StackSize, Traits, Alloc> operator+(CharT lhs, ff::basic_string<CharT, StackSize, Traits, Alloc>&& rhs);

template<class CharT, size_t StackSize, class Traits, class Alloc>
bool operator==(const ff::basic_string<CharT, StackSize, Traits, Alloc>& lhs, const ff::basic_string<CharT, StackSize, Traits, Alloc>& rhs) noexcept;

template<class CharT, size_t StackSize, class Traits, class Alloc>
bool operator!=(const ff::basic_string<CharT, StackSize, Traits, Alloc>& lhs, const ff::basic_string<CharT, StackSize, Traits, Alloc>& rhs) noexcept;

template<class CharT, size_t StackSize, class Traits, class Alloc>
bool operator<(const ff::basic_string<CharT, StackSize, Traits, Alloc>& lhs, const ff::basic_string<CharT, StackSize, Traits, Alloc>& rhs) noexcept;

template<class CharT, size_t StackSize, class Traits, class Alloc>
bool operator<=(const ff::basic_string<CharT, StackSize, Traits, Alloc>& lhs, const ff::basic_string<CharT, StackSize, Traits, Alloc>& rhs) noexcept;

template<class CharT, size_t StackSize, class Traits, class Alloc>
bool operator>(const ff::basic_string<CharT, StackSize, Traits, Alloc>& lhs, const ff::basic_string<CharT, StackSize, Traits, Alloc>& rhs) noexcept;

template<class CharT, size_t StackSize, class Traits, class Alloc>
bool operator>=(const ff::basic_string<CharT, StackSize, Traits, Alloc>& lhs, const ff::basic_string<CharT, StackSize, Traits, Alloc>& rhs) noexcept;

template<class CharT, size_t StackSize, class Traits, class Alloc>
bool operator==(const ff::basic_string<CharT, StackSize, Traits, Alloc>& lhs, const CharT* rhs);

template<class CharT, size_t StackSize, class Traits, class Alloc>
bool operator==(const CharT* lhs, const ff::basic_string<CharT, StackSize, Traits, Alloc>& rhs);

template<class CharT, size_t StackSize, class Traits, class Alloc>
bool operator!=(const ff::basic_string<CharT, StackSize, Traits, Alloc>& lhs, const CharT* rhs);

template<class CharT, size_t StackSize, class Traits, class Alloc>
bool operator!=(const CharT* lhs, const ff::basic_string<CharT, StackSize, Traits, Alloc>& rhs);

template<class CharT, size_t StackSize, class Traits, class Alloc>
bool operator<(const ff::basic_string<CharT, StackSize, Traits, Alloc>& lhs, const CharT* rhs);

template<class CharT, size_t StackSize, class Traits, class Alloc>
bool operator<(const CharT* lhs, const ff::basic_string<CharT, StackSize, Traits, Alloc>& rhs);

template<class CharT, size_t StackSize, class Traits, class Alloc>
bool operator<=(const ff::basic_string<CharT, StackSize, Traits, Alloc>& lhs, const CharT* rhs);

template<class CharT, size_t StackSize, class Traits, class Alloc>
bool operator<=(const CharT* lhs, const ff::basic_string<CharT, StackSize, Traits, Alloc>& rhs);

template<class CharT, size_t StackSize, class Traits, class Alloc>
bool operator>(const ff::basic_string<CharT, StackSize, Traits, Alloc>& lhs, const CharT* rhs);

template<class CharT, size_t StackSize, class Traits, class Alloc>
bool operator>(const CharT* lhs, const ff::basic_string<CharT, StackSize, Traits, Alloc>& rhs);

template<class CharT, size_t StackSize, class Traits, class Alloc>
bool operator>=(const ff::basic_string<CharT, StackSize, Traits, Alloc>& lhs, const CharT* rhs);

template<class CharT, size_t StackSize, class Traits, class Alloc>
bool operator>=(const CharT* lhs, const ff::basic_string<CharT, StackSize, Traits, Alloc>& rhs);

template<class CharT, size_t StackSize, class Traits, class Alloc>
std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, const ff::basic_string<CharT, StackSize, Traits, Alloc>& str);

template<class CharT, size_t StackSize, class Traits, class Alloc>
std::basic_istream<CharT, Traits>& operator>>(std::basic_istream<CharT, Traits>& is, ff::basic_string<CharT, StackSize, Traits, Alloc>& str);
#endif

#if 0
ff::string operator""s(const char* str, std::size_t len);
ff::u16string operator""s(const char16_t* str, std::size_t len);
ff::u32string operator""s(const char32_t* str, std::size_t len);
ff::wstring operator""s(const wchar_t* str, std::size_t len);
#endif
