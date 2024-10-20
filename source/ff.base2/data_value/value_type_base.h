#pragma once

#include "../base/assert.h"
#include "../types/stack_vector.h"
#include "../data_persist/persist.h"
#include "../data_value/value.h"
#include "../data_value/value_traits.h"
#include "../data_value/value_type.h"

namespace ff::internal
{
    template<class T>
    class value_type_base : public value_type
    {
    public:
        using value_derived_type = typename T;
        using get_type = typename ff::type::value_traits<T>::get_type;
        using raw_type = typename ff::type::value_traits<T>::raw_type;
        using this_type = typename value_type_base<T>;

        using value_type::value_type;

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

        virtual bool equals(const value* val1, const value* val2) const
        {
            return val1->get<value_derived_type>() == val2->get<value_derived_type>();
        }
    };

    template<class T>
    class value_type_simple : public value_type_base<T>
    {
    public:
        using value_type_base<T>::value_type_base;
        using raw_type = typename ff::internal::value_type_base<T>::raw_type;
        using get_type = typename ff::internal::value_type_base<T>::get_type;
        using value_derived_type = typename ff::internal::value_type_base<T>::value_derived_type;

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

    template<class T>
    class value_type_pod_vector : public value_type_base<T>
    {
    public:
        using value_type_base<T>::value_type_base;
        using raw_type = typename ff::internal::value_type_base<T>::raw_type;
        using get_type = typename ff::internal::value_type_base<T>::get_type;

        virtual value_ptr try_convert_from(const value* other) const override
        {
            if (other->can_have_indexed_children())
            {
                raw_type vec;
                vec.reserve(other->index_child_count());

                for (size_t i = 0; i < other->index_child_count(); i++)
                {
                    value_ptr val = other->index_child(i)->try_convert<typename raw_type::value_type>();
                    if (!val)
                    {
                        return {};
                    }

                    vec.push_back(val->get<typename raw_type::value_type>());
                }

                return ff::value::create<T>(std::move(vec));
            }

            return nullptr;
        }

        virtual bool can_have_indexed_children() const override
        {
            return true;
        }

        virtual value_ptr index_child(const value* val, size_t index) const override
        {
            get_type values = val->get<T>();
            return index < values.size() ? ff::value::create<raw_type::value_type>(values[index]) : nullptr;
        }

        virtual size_t index_child_count(const value* val) const override
        {
            return val->get<T>().size();
        }

        virtual ff::value_ptr load(reader_base& reader) const override
        {
            size_t size;
            if (ff::load(reader, size))
            {
                raw_type vec;
                if (size)
                {
                    vec.resize(size);
                    if (!ff::load_bytes(reader, vec.data(), ff::vector_byte_size(vec)))
                    {
                        assert(false);
                        return nullptr;
                    }
                }

                return ff::value::create<T>(std::move(vec));
            }

            assert(false);
            return nullptr;
        }

        virtual bool save(const value* val, writer_base& writer) const override
        {
            get_type src = val->get<T>();
            size_t size = src.size();
            if (ff::save(writer, size))
            {
                if (src.empty() || ff::save_bytes(writer, src.data(), ff::vector_byte_size(src)))
                {
                    return true;
                }
            }

            assert(false);
            return false;
        }
    };

    template<class T>
    class value_type_object_vector : public value_type_pod_vector<T>
    {
    public:
        using value_type_pod_vector<T>::value_type_pod_vector;
        using raw_type = typename ff::internal::value_type_base<T>::raw_type;
        using get_type = typename ff::internal::value_type_base<T>::get_type;

        virtual ff::value_ptr load(reader_base& reader) const override
        {
            size_t size;
            if (ff::load(reader, size))
            {
                raw_type vec;
                if (size)
                {
                    vec.reserve(size);

                    for (size_t i = 0; i < size; i++)
                    {
                        typename raw_type::value_type child;
                        if (!ff::load(reader, child))
                        {
                            assert(false);
                            return nullptr;
                        }

                        vec.push_back(std::move(child));
                    }

                    return ff::value::create<T>(std::move(vec));
                }
            }

            assert(false);
            return nullptr;
        }

        virtual bool save(const value* val, writer_base& writer) const override
        {
            get_type src = val->get<T>();
            size_t size = src.size();
            if (ff::save(writer, size))
            {
                for (const typename raw_type::value_type& child : src)
                {
                    if (!ff::save(writer, child))
                    {
                        assert(false);
                        return false;
                    }
                }

                return true;
            }

            assert(false);
            return false;
        }
    };
}
