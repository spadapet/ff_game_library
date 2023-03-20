#pragma once

#include "frame_allocator.h"

namespace ff
{
    template<class T>
    class stash
    {
    private:
        struct node_type : public ::SLIST_ENTRY, public T
        {};

    public:
        stash()
        {
            ::InitializeSListHead(&this->list_header);
        }

        stash(stash&& other) noexcept
        {
            ::InitializeSListHead(&this->list_header);
            *this = std::move(other);
        }

        stash& operator=(stash&& other) noexcept
        {
            if (this != &other)
            {
                this->allocator = std::move(other.allocator);
                this->refs = other.refs.load();
                other.refs = 0;

                for (::PSLIST_ENTRY entry = ::InterlockedPopEntrySList(&other.list_header);
                    entry; entry = ::InterlockedPopEntrySList(&other.list_header))
                {
                    ::InterlockedPushEntrySList(&this->list_header, entry);
                }
            }

            return *this;
        }

        stash(const stash& other) = delete;
        stash& operator=(const stash& other) = delete;

        ~stash()
        {
            assert(this->refs == 0);

            for (::PSLIST_ENTRY entry = ::InterlockedFlushSList(&this->list_header), next{}; entry; entry = next)
            {
                next = entry->Next;
                std::destroy_at(static_cast<node_type*>(entry));
            }
        }

        T* get_obj()
        {
            ::PSLIST_ENTRY entry = ::InterlockedPopEntrySList(&this->list_header);
            if (!entry)
            {
                node_type* node;
                {
                    std::scoped_lock lock(this->mutex);
                    node = this->allocator.alloc<node_type>();
                }

                entry = std::construct_at(node);
            }

            assert(this->refs.fetch_add(1) >= 0);
            return static_cast<node_type*>(entry);
        }

        void stash_obj(T* obj)
        {
            assert(this->refs.fetch_sub(1) > 0);
            ::InterlockedPushEntrySList(&this->list_header, static_cast<node_type*>(obj));
        }

    private:
        ::SLIST_HEADER list_header;
        ff::frame_allocator allocator;
        std::mutex mutex;
        std::atomic_int refs{0};
    };
}
