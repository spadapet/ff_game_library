#pragma once

#include "entity.h"

namespace ff
{
    class component_pool_base
    {
    public:
        using create_function = typename std::function<std::unique_ptr<component_pool_base>(std::type_index, uint64_t)>;

        component_pool_base(std::type_index type, uint64_t bit);
        virtual ~component_pool_base() = 0;

        template<class T, class... Args>
        T* set(ff::entity entity, bool& added, Args&&... args)
        {
            T* component = reinterpret_cast<T*>(this->internal_get_or_add_default(entity, added));
            if (added || sizeof...(Args) != 0)
            {
                *component = T(std::forward<Args>(args)...);
            }

            return component;
        }

        template<class T>
        T* get(ff::entity entity) const
        {
            return reinterpret_cast<T*>(this->internal_get(entity));
        }

        virtual bool remove(ff::entity entity) = 0;

        std::type_index type() const;
        uint64_t bit() const;

    protected:
        virtual void* internal_get_or_add_default(ff::entity entity, bool& added) = 0;
        virtual void* internal_get(ff::entity entity) const = 0;

    private:
        std::type_index type_;
        uint64_t bit_;
    };
}

namespace ff::internal
{
    template<class T>
    class component_pool : public ff::component_pool_base
    {
    public:
        using ff::component_pool_base::component_pool_base;

        virtual bool remove(ff::entity entity) override
        {
            return this->entity_to_component.erase(entity) != 0;
        }

        ff::signal<ff::entity, T&>& added()
        {
            return this->added_;
        }

        ff::signal<ff::entity, T&>& updated()
        {
            return this->updated_;
        }

        ff::signal<ff::entity, T&>& removing()
        {
            return this->removing_;
        }

    protected:
        virtual void* internal_get_or_add_default(ff::entity entity, bool& added) override
        {
            auto i = this->entity_to_component.try_emplace(entity);
            added = i.second;
            return &i.first->second;
        }

        virtual void* internal_get(ff::entity entity) const override
        {
            auto i = this->entity_to_component.find(entity);
            if (i != this->entity_to_component.cend())
            {
                return &i->second;
            }
        }

    private:
        ff::unordered_map<ff::entity, T> entity_to_component;
        ff::signal<ff::entity, T&> added_;
        ff::signal<ff::entity, T&> updated_;
        ff::signal<ff::entity, T&> removing_;
    };
}

namespace ff
{
    template<class T>
    class component_pool_traits
    {
    public:
        using pool_base_type = typename ff::component_pool_base;
        using pool_type = typename ff::internal::component_pool<T>;

        static std::unique_ptr<pool_base_type> create_pool(std::type_index type, uint64_t bit)
        {
            return std::make_unique<pool_type>(type, bit);
        }

        static constexpr ff::signal<ff::entity, T&>& added(ff::component_pool_base* pool)
        {
            return static_cast<pool_type*>(pool)->added();
        }

        static constexpr ff::signal<ff::entity, T&>& updated(ff::component_pool_base* pool)
        {
            return static_cast<pool_type*>(pool)->updated();
        }

        static constexpr ff::signal<ff::entity, T&>& removing(ff::component_pool_base* pool)
        {
            return static_cast<pool_type*>(pool)->removing();
        }
    };
}
