#pragma once

#include "../type/value_traits.h"

namespace ff::data
{
    class reader_base;
    class writer_base;

    class value;
    using value_ptr = typename ff::intrusive_ptr<const value>;

    class value_type
    {
    public:
        value_type();
        virtual ~value_type() = 0;

        // types
        virtual size_t size_of() const = 0;
        virtual void destruct(value* obj) const = 0;
        virtual std::type_index type_index() const = 0;
        virtual std::string_view type_name() const = 0;
        virtual uint32_t type_persist_id() const = 0;
        uint32_t type_lookup_id() const;

        // compare
        virtual bool equals(const value* val1, const value* val2) const;

        // convert
        virtual value_ptr try_convert_to(const value* val, std::type_index type) const;
        virtual value_ptr try_convert_from(const value* other) const;

        // maps
        virtual bool can_have_named_children() const;
        virtual value_ptr named_child(const value* val, std::string_view name) const;
        virtual std::vector<std::string> child_names(const value* val) const;

        // arrays
        virtual bool can_have_indexed_children() const;
        virtual value_ptr index_child(const value* val, size_t index) const;
        virtual size_t index_child_count(const value* val) const;

        // persist
        virtual value_ptr load(reader_base& reader) const;
        virtual bool save(const value* val, writer_base& writer) const;
        virtual void print(const value* val, std::ostream& output) const;
        virtual void print_tree(const value* val, std::ostream& output) const;

    private:
        uint32_t lookup_id;
    };
}
