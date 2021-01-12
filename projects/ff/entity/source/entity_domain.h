#pragma once

#include "component_pool.h"
#include "entity_data.h"

namespace ff
{
    class entity_domain
    {
    public:
        entity_domain();
        entity_domain(entity_domain&& other) noexcept = delete;
        entity_domain(const entity_domain& other) = delete;
        ~entity_domain();

        entity_domain& operator=(entity_domain&& other) noexcept = delete;
        entity_domain& operator=(const entity_domain& other) = delete;

        // Component functions
        template<class T, class... Args> T* set(ff::entity entity, Args&&... args)
        {
            bool added;
            ff::component_pool_base* pool = this->ensure_component_pool<T>();
            T* component = pool->set<T>(entity, added, std::forward<Args>(args...));

            if (added)
            {
                this->added_component(entity, pool, component);
                ff::component_pool_traits<T>::added(pool).notify(entity, *component);
            }
            else
            {
                ff::component_pool_traits<T>::updated(pool).notify(entity, *component);
            }

            return component;
        }

        template<class T> T* get(ff::entity entity) const
        {
            const ff::component_pool_base* pool = this->get_component_pool(typeid(T));
            return pool ? pool->get<T>(entity) : nullptr;
        }

        template<class T> bool remove(ff::entity entity)
        {
            ff::component_pool_base* pool = this->get_component_pool(typeid(T));
            T* component = pool ? pool->get<T>(entity) : nullptr;
            if (component)
            {
                ff::component_pool_traits<T>::removing(pool).notify(entity, *component);
                return pool->remove(entity)
            }

            return false;
        }

        // Entity functions (not activated by default)
        ff::entity create();
        ff::entity clone(ff::const_entity entity);
        void destroy(ff::entity entity);
        void destroy_all();
        bool active(ff::const_entity entity) const;
        void active(ff::entity entity, bool value);
        size_t hash(ff::const_entity entity) const;
        void event_notify(ff::entity entity, size_t event_id, void* event_args = nullptr);
        ff::entity_event_sink& event_sink(ff::entity entity, size_t event_id);

    private:
        ff::component_pool_base* get_component_pool(std::type_index type) const;
        ff::component_pool_base* add_component_pool(std::type_index type, const ff::component_pool_base::create_function& create_func);
        void added_component(ff::entity entity, ff::component_pool_base* pool, void* component);

        void register_entity_with_buckets(ff::internal::entity_data* entity_data, ff::component_pool_base* pool, void* component);

        template<class T>
        ff::component_pool_base* ensure_component_pool()
        {
            auto i = this->type_to_component_pool.find(typeid(T));
            return (i == this->type_to_component_pool.cend())
                ? this->add_component_pool(typeid(T), &ff::component_pool_traits<T>::create_pool)
                : i->second.get();
        }

        ff::list<ff::internal::entity_data> entities;
        ff::unordered_map<std::type_index, std::unique_ptr<ff::component_pool_base>> type_to_component_pool;
        size_t last_entity_hash;
    };
}
