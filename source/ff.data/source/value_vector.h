#pragma once

#include "value_type_base.h"
#include "value_vector_base.h"

namespace ff::type
{
    using value_vector_v = ff::internal::value_vector_base<value_ptr>;

    template<>
    struct value_traits<ff::value_vector> : public value_derived_traits<ff::type::value_vector_v>
    {};

    class value_vector_type : public ff::internal::value_type_base<ff::type::value_vector_v>
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
