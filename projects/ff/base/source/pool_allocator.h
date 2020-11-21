#pragma once

#include "math.h"

namespace ff::internal
{
    template<class T>
    struct byte_pool
    {
        union alignas(::SLIST_ENTRY) alignas(T) alignas(std::max_align_t) node_type
        {
            ::SLIST_ENTRY entry;
            std::array<uint8_t, sizeof(T)> item;
        };

        byte_pool(size_t size, ::PSLIST_ENTRY& first_entry, ::PSLIST_ENTRY& last_entry)
            : size(std::max<size_t>(ff::math::nearest_power_of_two(size), 8))
            , nodes(std::make_unique<node_type[]>(this->size))
        {
            for (size_t i = 0; i < this->size - 1; i++)
            {
                this->nodes[i].entry.Next = &this->nodes[i + 1].entry;
            }

            this->nodes[this->size - 1].entry.Next = nullptr;

            first_entry = &this->nodes[0].entry;
            last_entry = &this->nodes[this->size - 1].entry;
        }

        std::unique_ptr<byte_pool<T>>& last_pool(size_t& previous_size)
        {
            previous_size = this->size;
            return !this->next_pool ? this->next_pool : this->next_pool->last_pool(previous_size);
        }

        size_t size;
        std::unique_ptr<node_type[]> nodes;
        std::unique_ptr<byte_pool<T>> next_pool;
    };
}

namespace ff
{
    /// <summary>
    /// Allocates buffers of a single size and alignment from a reusable memory pool
    /// </summary>
    /// <remarks>
    /// This byte pool allocator can be thread safe or not. The thread safe code is lock free during
    /// normal calls to new_bytes and delete_bytes. Only when new_bytes needs to allocate more pool memory
    /// is a lock briefly taken. Generally you should use the ff::pool_allocator class instead.
    /// </remarks>
    /// <typeparam name="T">Buffer size and alignment are based on the size of this type</typeparam>
    /// <typeparam name="ThreadSafe">true if calls into this pool need to be thread safe</typeparam>
    template<class T, bool ThreadSafe = true>
    class byte_pool_allocator
    {
    public:
        using this_type = typename byte_pool_allocator<T, ThreadSafe>;
        using pool_type = typename ff::internal::byte_pool<T>;

        byte_pool_allocator()
            : size(0)
        {
            ::InitializeSListHead(&this->free_list);
        }

        byte_pool_allocator(this_type&& other)
            : pool_list(std::move(other.pool_list))
            , free_list(other.free_list)
            , size(other.size.load())
        {
            ::InitializeSListHead(&other.free_list);
            other.size = 0;
        }

        ~byte_pool_allocator()
        {
            assert(!this->size);
            ::InterlockedFlushSList(&this->free_list);
        }

        this_type& operator=(this_type&& other)
        {
            if (this != &other)
            {
                assert(!this->size);

                std::scoped_lock lock(this->mutex, other.mutex);

                this->pool_list = std::move(other.pool_list);
                this->free_list = other.free_list;
                this->size = other.size;

                ::InitializeSListHead(&other.free_list);
                other.size = 0;
            }


            return *this;
        }

        void* new_bytes()
        {
            ::PSLIST_ENTRY free_entry = nullptr;
            while (!free_entry && !(free_entry = ::InterlockedPopEntrySList(&this->free_list)))
            {
                std::lock_guard lock(this->mutex);

                if (!(free_entry = ::InterlockedPopEntrySList(&this->free_list)))
                {
                    size_t previous_size = 0;
                    std::unique_ptr<pool_type>& last_pool = !this->pool_list ? this->pool_list : this->pool_list->last_pool(previous_size);
                    ::PSLIST_ENTRY first_entry = nullptr, last_entry = nullptr;
                    last_pool.reset(new pool_type(previous_size * 2, first_entry, last_entry));

                    ::InterlockedPushListSListEx(&this->free_list, first_entry, last_entry, static_cast<ULONG>(last_pool->size));
                }
            }

            this->size.fetch_add(1);
            return free_entry;
        }

        void delete_bytes(void* obj)
        {
            if (obj)
            {
                assert(this->size);

                typename pool_type::node_type* node = reinterpret_cast<typename pool_type::node_type*>(obj);
                ::InterlockedPushEntrySList(&this->free_list, &node->entry);
                this->size.fetch_sub(1);
            }
        }

        void reduce_if_empty()
        {
            if (!this->size)
            {
                std::lock_guard lock(this->mutex);
                if (!this->size)
                {
                    ::InterlockedFlushSList(&this->free_list);
                    this->pool_list.reset();
                }
            }
        }

        void get_stats(size_t* size, size_t* allocated) const
        {
            std::lock_guard lock(this->mutex);

            if (size)
            {
                *size = this->size;
            }

            if (allocated)
            {
                *allocated = 0;

                for (std::unique_ptr<pool_type>* i = &this->pool_list; *i; i = &(*i)->next_pool)
                {
                    *allocated += (*i)->size;
                }
            }
        }

    private:
        byte_pool_allocator(const this_type& other) = delete;
        byte_pool_allocator& operator=(const this_type& other) = delete;

        std::mutex mutex;
        std::unique_ptr<pool_type> pool_list;
        std::atomic_size_t size;
        ::SLIST_HEADER free_list;
    };

    template<class T>
    class byte_pool_allocator<T, false>
    {
    public:
        using this_type = typename byte_pool_allocator<T, false>;
        using pool_type = typename ff::internal::byte_pool<T>;

        byte_pool_allocator()
            : first_free(nullptr)
            , size(0)
        {
        }

        byte_pool_allocator(byte_pool_allocator&& other) noexcept
            : pool_list(std::move(other.pool_list))
            , first_free(other.first_free)
            , size(other.size)
        {
            other.first_free = nullptr;
            other.size = 0;
        }

        ~byte_pool_allocator()
        {
            assert(!this->size);
        }

        this_type& operator=(this_type&& other) noexcept
        {
            if (this != &other)
            {
                assert(!this->size);

                this->pool_list = std::move(other.pool_list);
                this->first_free = other.first_free;
                this->size = other.size;

                other.first_free = nullptr;
                other.size = 0;
            }

            return *this;
        }

        void* new_bytes()
        {
            if (!this->first_free)
            {
                size_t previous_size = 0;
                std::unique_ptr<pool_type>& last_pool = !this->pool_list ? this->pool_list : this->pool_list->last_pool(previous_size);
                ::PSLIST_ENTRY last_entry;
                last_pool.reset(new pool_type(previous_size * 2, this->first_free, last_entry));
            }

            ::PSLIST_ENTRY free_entry = this->first_free;
            this->first_free = this->first_free->Next;
            this->size++;

            return free_entry;
        }

        void delete_bytes(void* obj)
        {
            if (obj)
            {
                typename pool_type::node_type* node = reinterpret_cast<typename pool_type::node_type*>(obj);
                node->entry.Next = this->first_free;
                this->first_free = &node->entry;
                this->size--;
            }
        }

        void reduce_if_empty()
        {
            if (!this->size)
            {
                this->first_free = nullptr;
                this->pool_list.reset();
            }
        }

        void get_stats(size_t* size, size_t* allocated) const
        {
            if (size)
            {
                *size = this->size;
            }

            if (allocated)
            {
                *allocated = 0;

                for (const std::unique_ptr<pool_type>* i = &this->pool_list; *i; i = &(*i)->next_pool)
                {
                    *allocated += (*i)->size;
                }
            }
        }

    private:
        byte_pool_allocator(const byte_pool_allocator& other) = delete;
        byte_pool_allocator& operator=(const byte_pool_allocator& other) = delete;

        std::unique_ptr<pool_type> pool_list;
        ::PSLIST_ENTRY first_free;
        size_t size;
    };

    /// <summary>
    /// Allocates objects of a single type from a reusable memory pool
    /// </summary>
    /// <remarks>
    /// This pool allocator can be thread safe or not. The thread safe code is lock free during
    /// normal calls to new_obj and delete_obj. Only when new_obj needs to allocate more pool memory
    /// is a lock briefly taken.
    /// </remarks>
    /// <typeparam name="T">Object type</typeparam>
    /// <typeparam name="ThreadSafe">true if calls into this pool need to be thread safe</typeparam>
    template<class T, bool ThreadSafe = true>
    class pool_allocator
    {
    public:
        using this_type = typename pool_allocator<T, ThreadSafe>;

        pool_allocator() = default;
        pool_allocator(this_type&& other) noexcept = default;

        this_type& operator=(this_type&& other) noexcept
        {
            this->byte_allocator = std::move(other.byte_allocator);
            return *this;
        }

        template<class... Args> T* new_obj(Args&&... args)
        {
            return ::new(this->byte_allocator.new_bytes()) T(std::forward<Args>(args)...);
        }

        void delete_obj(T* obj)
        {
            if (obj)
            {
                obj->~T();
                this->byte_allocator.delete_bytes(obj);
            }
        }

        void reduce_if_empty()
        {
            this->byte_allocator.reduce_if_empty();
        }

        void get_stats(size_t* size, size_t* allocated) const
        {
            this->byte_allocator.get_stats(size, allocated);
        }

    private:
        pool_allocator(const this_type& other) = delete;
        pool_allocator& operator=(const this_type& other) = delete;

        byte_pool_allocator<T, ThreadSafe> byte_allocator;
    };
}

namespace std
{
    template<class T, bool TS>
    void swap(ff::pool_allocator<T, TS>& lhs, ff::pool_allocator<T, TS>& other) noexcept
    {
        if (&lhs != &other)
        {
            ff::pool_allocator<T, TS> temp = std::move(lhs);
            lhs = std::move(other);
            other = std::move(temp);
        }
    }

    template<class T, bool TS>
    void swap(ff::byte_pool_allocator<T, TS>& lhs, ff::byte_pool_allocator<T, TS>& other)
    {
        if (&lhs != &other)
        {
            ff::byte_pool_allocator<T, TS> temp = std::move(lhs);
            lhs = std::move(other);
            other = std::move(temp);
        }
    }
}

