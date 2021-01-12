#pragma once

#include "entity.h"

namespace ff
{
    class component_pool_base;
    class entity_domain;
}

namespace ff::internal
{
    class entity_data : public ff::entity_base
    {
    public:
        entity_data(ff::entity_domain* domain, size_t hash);
        entity_data(entity_data&& other) noexcept = delete;
        entity_data(const entity_data& other) = delete;

        entity_data& operator=(entity_data&& other) noexcept = delete;
        entity_data& operator=(const entity_data& other) = delete;

        static entity_data* get(ff::entity entity);
        static entity_data const* get(ff::const_entity entity);

        std::vector<component_pool_base*> components_;
        // std::vector<> event_handlers;
        // std::vector<> buckets;
        uint64_t component_bits_;
        size_t hash_;
        bool active_;
    };
}
