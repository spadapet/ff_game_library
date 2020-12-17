#pragma once

#include "resource_object_ptr.h"

namespace ff
{
    class resource_load_context;

    class resource_object_type
    {
    public:
        virtual ~resource_object_type() = 0;

        virtual std::type_index type_index() const = 0;
        std::string_view type_name() const;
        uint32_t type_lookup_id() const;

        virtual resource_object_ptr load_from_source(const ff::dict& dict, resource_load_context& context) const;
        virtual resource_object_ptr load_from_cache(const ff::dict& dict) const;
    };
}
