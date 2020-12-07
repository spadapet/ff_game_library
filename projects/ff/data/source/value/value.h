#pragma once

#include "value_allocator.h"
#include "value_type.h"
#include "../type/value_traits.h"

namespace ff
{
    class value
    {
    public:
        template<class Type>
        static void register_type()
        {
            ff::value::register_type(std::make_unique<Type>());
        }

        template<typename T, typename... Args>
        static value_ptr create(Args&&... args)
        {
            using value_derived_type = typename ff::type::value_traits<T>::value_derived_type;
            value_derived_type* val = static_cast<value_derived_type*>(value_derived_type::get_static_value(std::forward<Args>(args)...));;
            if (!val)
            {
                val = reinterpret_cast<value_derived_type*>(ff::internal::value_allocator::new_bytes(sizeof(value_derived_type)));
                ::new(val) value_derived_type(std::forward<Args>(args)...);
                val->data.refs++;
            }

            const value_type* type = ff::value::get_type(typeid(value_derived_type));
            val->data.type_lookup_id = type->type_lookup_id();
            return val;
        }

        template<typename T>
        static value_ptr create_default()
        {
            using value_derived_type = typename ff::type::value_traits<T>::value_derived_type;
            const value_type* type = ff::value::get_type(typeid(value_derived_type));

            value* val = value_derived_type::get_static_default_value();
            val->data.type_lookup_id = type->type_lookup_id();
            return val;
        }

        template<class T>
        auto get() const -> typename ff::type::value_traits<T>::get_type
        {
            using value_derived_type = typename ff::type::value_traits<T>::value_derived_type;
            const value* val = this->is_type<value_derived_type>() ? this : value_derived_type::get_static_default_value();
            return static_cast<const value_derived_type*>(val)->get();
        }

        template<class T>
        bool is_type() const
        {
            return this->is_type(typeid(ff::type::value_traits<T>::value_derived_type));
        }

        template<class T>
        value_ptr try_convert() const
        {
            return this->try_convert(typeid(ff::type::value_traits<T>::value_derived_type));
        }

        template<class T>
        value_ptr convert_or_default() const
        {
            value_ptr val = this->try_convert<T>();
            return val ? val : ff::value::create_default<T>();
        }

        // maps
        bool can_have_named_children() const;
        value_ptr named_child(std::string_view name) const;
        std::vector<std::string> child_names() const;

        // arrays
        bool can_have_indexed_children() const;
        value_ptr index_child(size_t index) const;
        size_t index_child_count() const;

        // persist
        static value_ptr load_typed(reader_base& reader);
        bool save_typed(writer_base& writer) const;
        void print(std::ostream& output) const;
        void print_tree(std::ostream& output) const;
        void debug_print_tree() const;

        bool equals(const value* other) const;
        const value_type* type() const;
        std::type_index type_index() const;
        std::string_view type_name() const;
        uint32_t type_lookup_id() const;
        uint32_t type_persist_id() const;

        // intrusive ref count
        void add_ref() const;
        void release_ref() const;

    protected:
        value();
        ~value();

    private:
        static bool register_type(std::unique_ptr<value_type>&& type);
        static const value_type* get_type(std::type_index type_index);
        static const value_type* get_type_by_lookup_id(uint32_t id);
        static const value_type* get_type_by_persist_id(uint32_t id);

        bool is_type(std::type_index type_index) const;
        value_ptr try_convert(std::type_index type_index) const;

        union
        {
            mutable uint32_t type_and_ref;

            struct
            {
                uint32_t refs : 24;
                uint32_t type_lookup_id : 8;
            };
        } data;
    };

    template<class T>
    class value_vector_base : public value
    {
    public:
        value_vector_base(std::vector<T>&& value)
            : value(std::move(value))
        {}

        const std::vector<T>& get() const
        {
            return this->value;
        }

        static ff::value* get_static_value(std::vector<T>&& value)
        {
            return value.empty() ? get_static_default_value() : nullptr;
        }

        static ff::value* get_static_default_value()
        {
            static value_vector_base<T> empty = std::vector<T>();
            return &empty;
        }

    private:
        std::vector<T> value;
    };
}
