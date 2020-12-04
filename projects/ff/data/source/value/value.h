#pragma once

#include "value_allocator.h"
#include "value_type.h"

namespace ff::data
{
    class value
    {
    public:
        template<class Type>
        static void register_type()
        {
            ff::data::value::register_type(std::make_unique<Type>());
        }

        template<typename T, typename... Args>
        static value_ptr create(Args&&... args)
        {
            T* val = static_cast<T*>(T::get_static_value(std::forward<Args>(args)...));;
            if (!val)
            {
                val = reinterpret_cast<T*>(ff::data::internal::value_allocator::new_bytes(sizeof(T)));
                ::new(val) T(std::forward<Args>(args)...);
                val->data.refs++;
            }

            if (!val->data.type_lookup_id)
            {
                value_type* type = ff::data::value::get_type(typeid(T));
                assert(type);
                val->data.type_lookup_id = type->type_lookup_id();
            }

            return val;
        }

        template<typename T>
        static value_ptr create_default()
        {
            const value* val = T::get_static_default_value();
            if (val && !val->type)
            {
                val->type = ff::data::value::get_type(typeid(T));
            }

            return val;
        }

        template<class T>
        auto get() const -> typename std::invoke_result_t<decltype(&T::get), T>
        {
            const value* val = this->is_type<T>() ? this : T::get_static_default_value();
            return static_cast<const T*>(val)->get();
        }

        template<class T>
        bool is_type() const
        {
            return this->is_type(typeid(T));
        }

        template<class T>
        value_ptr try_convert() const
        {
            return this->try_convert(typeid(T));
        }

        template<class T>
        value_ptr convert_or_default() const
        {
            value_ptr val = this->try_convert<T>();
            return val ? val : ff::data::value::create_default();
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
        bool save_typed(writer_base& writer);
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
        bool is_type(std::type_index type_index);
        value_ptr try_convert(std::type_index type_index);

        union
        {
            mutable uint32_t type_and_ref;

            struct
            {
                uint32_t type_lookup_id : 8;
                uint32_t refs : 24;
            };
        } data;
    };
}
