#include "pch.h"
#include "entity_data.h"

ff::internal::entity_data::entity_data(ff::entity_domain* domain, size_t hash)
    : ff::entity_base(domain)
    , component_bits_(0)
    , hash_(hash)
    , active_(false)
{
}

ff::internal::entity_data* ff::internal::entity_data::get(ff::entity entity)
{
    return static_cast<ff::internal::entity_data*>(entity);
}

ff::internal::entity_data const* ff::internal::entity_data::get(ff::const_entity entity)
{
    return static_cast<ff::internal::entity_data const*>(entity);
}
