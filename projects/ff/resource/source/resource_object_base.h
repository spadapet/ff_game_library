#pragma once

#include "resource_object_ptr.h"
#include "resource_value_ref.h"

namespace ff
{
    class resource_object_base
    {
    public:
        virtual ~resource_object_base() = 0;

        virtual bool save_to_cache(ff::dict& dict) const = 0;
        static bool save_to_cache_typed(const resource_object_base& value, ff::dict& dict);
        static resource_object_ptr load_from_cache_typed(const ff::dict& dict);

        virtual bool resource_load_from_source_complete();
        virtual std::vector<ff::resource_value_ref_ptr> resource_get_dependencies() const;
        virtual ff::dict resource_get_siblings(ff::resource_value_ref_ptr parent_ref) const;

        virtual bool resource_can_save_to_file() const;
        virtual std::string resource_get_file_extension() const;
        virtual bool resource_save_to_file(std::string_view path) const;
    };
}
