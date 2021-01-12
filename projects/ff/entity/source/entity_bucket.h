#pragma once

#include "component_pool.h"

namespace ff
{
    // For optional components in a bucket_entry template
    template<class T>
    struct optional_component {};

    class bucket_entry_base
    {
    public:
        bucket_entry_base()
            : entity_(nullptr)
        {}

        ff::entity entity() const
        {
            return this->entity_;
        }

        template<class T>
        T* get() const
        {
            return this->entity_->get<T>();
        }

        struct component_entry
        {
            const component_entry* next;
            std::type_index type;
            ff::component_pool_base::create_function create_pool;
            size_t offset;
            bool required;
        };

        static const component_entry* component_entries()
        {
            return nullptr;
        }

    protected:
        template<typename T>
        struct component_accessor
        {
            using component_type = typename T;
            static constexpr bool required = true;
        };

        template<typename T>
        struct component_accessor<optional_component<T>>
        {
            using component_type = typename T;
            static constexpr bool required = false;
        };

    private:
        ff::entity entity_;
    };

    template<typename... Types>
    class bucket_entry
    {
        // only specializations are used
    };

    template<>
    class bucket_entry<> : public bucket_entry_base
    {
        // end of recursion
    };

    template<typename FirstType, typename... NextTypes>
    class bucket_entry<FirstType, NextTypes...> : public bucket_entry<NextTypes...>
    {
    private:
        using base_type = typename bucket_entry<NextTypes...>;
        using first_component_type = typename bucket_entry_base::component_accessor<FirstType>::component_type;

    public:
        bucket_entry()
            : component_(nullptr)
        {}

        static const bucket_entry_base::component_entry* component_entries()
        {
            using this_type = typename bucket_entry<FirstType, NextTypes...>;

            static const bucket_entry_base::component_entry entry
            {
                base_type:: component_entries(),
                std::type_index(typeid(first_component_type)),
                &ff::component_pool_traits<first_component_type>::create_pool,
                reinterpret_cast<uint8_t*>(&reinterpret_cast<this_type*>(256)->component_) - reinterpret_cast<uint8_t*>(static_cast<bucket_entry_base*>(reinterpret_cast<this_type*>(256))),
                bucket_entry_base::ComponentAccessor<FirstType>::required,
            };

            return &entry;
        }

        template<typename T>
        T* get() const
        {
            return base_type::get<T>();
        }

        template<>
        first_component_type* get<first_component_type>() const
        {
            return component_;
        }

    private:
        first_component_type* component_;
    };

    class entity_bucket_base
    {
    public:
        virtual ~entity_bucket_base() = 0;
    };

    template<class T>
    class entity_bucket : public entity_bucket_base
    {
    public:
        virtual const ff::list<T>& entries() const = 0;
        virtual T* get(ff::entity entity) const = 0;
        virtual ff::signal_sink<T&, bool>& sink() = 0;
    };
}
