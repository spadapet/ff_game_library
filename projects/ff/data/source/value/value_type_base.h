#pragma once

#include "value_type.h"
#include "../persist.h"

namespace ff
{
    template<class T>
    class value_type_base : public value_type
    {
    public:
        using value_derived_type = typename T;
        using get_type = typename ff::type::value_traits<T>::get_type;
        using raw_type = typename ff::type::value_traits<T>::raw_type;
        using this_type = typename value_type_base<T>;

        virtual size_t size_of() const override
        {
            return sizeof(value_derived_type);
        }

        virtual void destruct(value* obj) const override
        {
            static_cast<value_derived_type*>(obj)->~value_derived_type();
        }

        virtual std::type_index type_index() const override
        {
            return typeid(value_derived_type);
        }

        virtual std::string_view type_name() const override
        {
            const char* name = typeid(value_derived_type).name();
            return std::string_view(name);
        }

        virtual uint32_t type_persist_id() const override
        {
            size_t id = ff::hash<std::string_view>()(this->type_name());
            return static_cast<uint32_t>(id);
        }

        virtual bool equals(const value* val1, const value* val2) const
        {
            assert(val1->type() == val2->type());
            return val1->get<value_derived_type>() == val2->get<value_derived_type>();
        }

        virtual ff::value_ptr load(reader_base& reader) const override
        {
            raw_type data;
            return ff::load(reader, data) ? ff::value::create<value_derived_type>(std::move(data)) : nullptr;
        }

        virtual bool save(const value* val, writer_base& writer) const override
        {
            get_type data = val->get<value_derived_type>();
            return ff::save(writer, data);
        }
    };
}
