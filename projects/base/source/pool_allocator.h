#pragma once

#include "math.h"
#include "mutex.h"

namespace ff::internal
{
    template<size_t ByteSize, size_t ByteAlign>
    struct byte_pool
    {
        union alignas(::SLIST_ENTRY) alignas(ByteAlign) node_type
        {
            ::SLIST_ENTRY entry;
            std::array<uint8_t, ByteSize> item;
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

        std::unique_ptr<byte_pool<ByteSize, ByteAlign>>& last_pool(size_t& previous_size)
        {
            previous_size = this->size;
            return !this->next_pool ? this->next_pool : this->next_pool->last_pool(previous_size);
        }

        size_t size;
        std::unique_ptr<node_type[]> nodes;
        std::unique_ptr<byte_pool<ByteSize, ByteAlign>> next_pool;
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
    /// <typeparam name="ByteSize">Size of the byte buffers</typeparam>
    /// <typeparam name="ByteAlign">Alignment of the byte buffers</typeparam>
    /// <typeparam name="ThreadSafe">true if calls into this pool need to be thread safe</typeparam>
    template<size_t ByteSize, size_t ByteAlign, bool ThreadSafe = true>
    class byte_pool_allocator
    {
        typedef ff::internal::byte_pool<ByteSize, ByteAlign> pool_type;

    public:
        byte_pool_allocator()
            : size(0)
        {
            ::InitializeSListHead(&this->free_list);
        }

        byte_pool_allocator(byte_pool_allocator&& rhs)
            : pool_list(std::move(rhs.pool_list))
            , free_list(rhs.free_list)
            , size(rhs.size.load())
        {
            ::InitializeSListHead(&rhs.free_list);
            rhs.size = 0;
        }

        ~byte_pool_allocator()
        {
            assert(!this->size);
            ::InterlockedFlushSList(&this->free_list);
        }

        void* new_bytes()
        {
            ::PSLIST_ENTRY free_entry = nullptr;
            while (!free_entry && !(free_entry = ::InterlockedPopEntrySList(&this->free_list)))
            {
                ff::lock_guard lock(mutex);

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

                pool_type::node_type* node = reinterpret_cast<pool_type::node_type*>(obj);
                ::InterlockedPushEntrySList(&this->free_list, &node->entry);
                this->size.fetch_sub(1);
            }
        }

        void reduce_if_empty()
        {
            if (!this->size)
            {
                ff::lock_guard lock(mutex);
                if (!this->size)
                {
                    ::InterlockedFlushSList(&this->free_list);
                    this->pool_list.reset();
                }
            }
        }

        void get_stats(size_t& size, size_t& allocated)
        {
            ff::lock_guard lock(mutex);
            size = this->size;
            allocated = 0;

            for (std::unique_ptr<pool_type>* i = &this->pool_list; *i; i = &(*i)->next_pool)
            {
                allocated += (*i)->size;
            }
        }

    private:
        byte_pool_allocator(const byte_pool_allocator& rhs) = delete;
        byte_pool_allocator& operator=(const byte_pool_allocator& rhs) = delete;

        ff::recursive_mutex mutex;
        std::unique_ptr<pool_type> pool_list;
        ::SLIST_HEADER free_list;
        std::atomic_size_t size;
    };

    template<size_t ByteSize, size_t ByteAlign>
    class byte_pool_allocator<ByteSize, ByteAlign, false>
    {
        typedef ff::internal::byte_pool<ByteSize, ByteAlign> pool_type;

    public:
        byte_pool_allocator()
            : first_free(nullptr)
            , size(0)
        {
        }

        byte_pool_allocator(byte_pool_allocator&& rhs)
            : pool_list(std::move(rhs.pool_list))
            , first_free(rhs.first_free)
            , size(rhs.size)
        {
            rhs.first_free = nullptr;
            rhs.size = 0;
        }

        ~byte_pool_allocator()
        {
            assert(!this->size);
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
                pool_type::node_type* node = reinterpret_cast<pool_type::node_type*>(obj);
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

        void get_stats(size_t& size, size_t& allocated)
        {
            size = this->size;
            allocated = 0;

            for (std::unique_ptr<pool_type>* i = &this->pool_list; *i; i = &(*i)->next_pool)
            {
                allocated += (*i)->size;
            }
        }

    private:
        byte_pool_allocator(const byte_pool_allocator& rhs) = delete;
        byte_pool_allocator& operator=(const byte_pool_allocator& rhs) = delete;

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
        pool_allocator()
        {
        }

        pool_allocator(pool_allocator&& rhs)
            : byte_allocator(std::move(rhs.byte_allocator))
        {
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

        void get_stats(size_t& size, size_t& allocated)
        {
            this->byte_allocator.get_stats(size, allocated);
        }

    private:
        pool_allocator(const pool_allocator& rhs) = delete;
        pool_allocator& operator=(const pool_allocator& rhs) = delete;

        byte_pool_allocator<sizeof(T), alignof(T), ThreadSafe> byte_allocator;
    };
}
