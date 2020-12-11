#pragma once

#include "value.h"
#include "value_type_base.h"

namespace ff::type
{
    using value_vector = ff::value_vector_base<value_ptr>;

    template<>
    struct value_traits<std::vector<value_ptr>> : public value_derived_traits<ff::type::value_vector>
    {};

    class value_vector_type : public ff::value_type_base<ff::type::value_vector>
    {
    public:
        using value_type_base::value_type_base;

        virtual value_ptr try_convert_from(const value* other) const override;

        virtual bool can_have_indexed_children() const override;
        virtual value_ptr index_child(const value* val, size_t index) const override;
        virtual size_t index_child_count(const value* val) const override;

        virtual value_ptr load(reader_base& reader) const override;
        virtual bool save(const value* val, writer_base& writer) const override;
    };
}
