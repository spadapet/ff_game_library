#include "pch.h"
#include "entity_domain.h"
#include "entity_events.h"

ff::entity_domain::entity_domain()
    : last_entity_hash(0)
{
}

ff::entity_domain::~entity_domain()
{
    this->destroy_all();
}

ff::entity ff::entity_domain::create()
{
    return &this->entities.emplace_back(this, ++this->last_entity_hash);
}

ff::entity ff::entity_domain::clone(ff::const_entity entity)
{
    return ff::entity();
}

void ff::entity_domain::destroy(ff::entity entity)
{
    ff::internal::entity_data* entity_data = ff::internal::entity_data::get(entity);
    this->active(entity, false);
    this->event_notify(entity, ff::entity_events::event_destroying);

    for (const ComponentFactoryEntry* factoryEntry : entityEntry->_components)
    {
        factoryEntry->_factory->Delete(entityEntry);
    }

    _entities.Delete(*entityEntry);
}

void ff::entity_domain::destroy_all()
{}

bool ff::entity_domain::active(ff::const_entity entity) const
{
    return false;
}

void ff::entity_domain::active(ff::entity entity, bool value)
{}

size_t ff::entity_domain::hash(ff::const_entity entity) const
{
    return size_t();
}

void ff::entity_domain::event_notify(ff::entity entity, size_t event_id, void* event_args)
{
    // TODO
}

ff::entity_event_sink& ff::entity_domain::event_sink(ff::entity entity, size_t event_id)
{
    // TODO: insert return statement here
}

ff::component_pool_base* ff::entity_domain::get_component_pool(std::type_index type) const
{
    auto i = this->type_to_component_pool.find(type);
    return (i != this->type_to_component_pool.end()) ? i->second.get() : nullptr;
}

ff::component_pool_base* ff::entity_domain::add_component_pool(std::type_index type, const ff::component_pool_base::create_function& create_func)
{
	auto i = this->type_to_component_pool.find(type);
	if (i == this->type_to_component_pool.end())
	{
        uint64_t bit = static_cast<uint64_t>(1) << (this->type_to_component_pool.size() % 64);
        i = this->type_to_component_pool.try_emplace(type, create_func(type, bit)).first;
	}

	return i->second.get();
}

void ff::entity_domain::added_component(ff::entity entity, ff::component_pool_base* pool, void* component)
{
    ff::internal::entity_data* entity_data = ff::internal::entity_data::get(entity);
    assert(std::find(entity_data->components.cbegin(), entity_data->components.cend(), pool) == entity_data->components.cend());

    entity_data->component_bits |= pool->bit();
    entity_data->components.push_back(pool);
    
    this->register_entity_with_buckets(entity_data, pool, component);
}

void ff::entity_domain::register_entity_with_buckets(ff::internal::entity_data* entity_data, ff::component_pool_base* pool, void* component)
{
    // TODO
}
