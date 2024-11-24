#pragma once

namespace ff::intrusive_list
{
    template<class T>
    class data
    {
    public:
        data() = default;
        data(data&& other) noexcept {}
        data(const data& other) = delete;

        ~data()
        {
            assert(!this->intrusive_next_ && !this->intrusive_prev_);
        }

        data& operator=(data&& other) noexcept { return *this; }
        data& operator=(const data& other) = delete;

        T* intrusive_next_{};
        T* intrusive_prev_{};
    };

    template<class T, class = std::enable_if_t<std::is_base_of_v<ff::intrusive_list::data<T>, T>>>
    void add_back(T*& list_front, T*& list_back, T* item)
    {
        assert(!item->intrusive_next_ && !item->intrusive_prev_);

        if (list_back)
        {
            item->intrusive_prev_ = list_back;
            list_back->intrusive_next_ = item;
        }

        list_back = item;

        if (!list_front)
        {
            list_front = item;
        }
    }

    template<class T, class = std::enable_if_t<std::is_base_of_v<ff::intrusive_list::data<T>, T>>>
    void add_front(T*& list_front, T*& list_back, T* item)
    {
        assert(!item->intrusive_next_ && !item->intrusive_prev_);

        if (list_front)
        {
            item->intrusive_next_ = list_front;
            list_front->intrusive_prev_ = item;
        }

        list_front = item;

        if (!list_back)
        {
            list_back = item;
        }
    }

    template<class T, class = std::enable_if_t<std::is_base_of_v<ff::intrusive_list::data<T>, T>>>
    void remove(T*& list_front, T*& list_back, T* item)
    {
        assert(item->intrusive_next_ || item->intrusive_prev_ || (item == list_front && item == list_back));

        T* prev = item->intrusive_prev_;
        T* next = item->intrusive_next_;

        if (item == list_back)
        {
            list_back = prev;
        }

        if (item == list_front)
        {
            list_front = next;
        }

        if (prev)
        {
            prev->intrusive_next_ = next;
        }

        if (next)
        {
            next->intrusive_prev_ = prev;
        }

        item->intrusive_next_ = nullptr;
        item->intrusive_prev_ = nullptr;
    }

    template<class T>
    size_t count(const ff::intrusive_list::data<T>* list_front)
    {
        size_t count = 0;

        for (const ff::intrusive_list::data<T>* i = list_front; i; i = i->intrusive_next_)
        {
            count++;
        }

        return count;
    }
}
