#pragma once

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
        virtual std::type_index alternate_type_index() const = 0;
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
        virtual void print(std::ostream& output) const;
        virtual void print_tree(std::ostream& output) const;

    private:
        uint32_t lookup_id;
    };

    template<class T>
    class value_type_base : public value_type
    {
    public:
        using value_type = typename T;
        using this_type = typename value_type_base<T>;

        value_type_base()
            : persist_id(0)
        {
        }

        virtual size_t size_of() const override
        {
            return sizeof(T);
        }

        virtual void destruct(value* obj) const override
        {
            static_cast<T*>(obj)->~T();
        }

        virtual std::type_index type_index() const override
        {
            return typeid(T);
        }

        virtual std::type_index alternate_type_index() const override
        {
            using alternate_type = typename std::remove_cv_t<typename std::remove_reference_t<typename std::invoke_result_t<decltype(&T::get), T>>>;
            return typeid(alternate_type);
        }

        virtual std::string_view type_name() const override
        {
            const char* name = typeid(T).name();
            return std::string_view(name);
        }

        virtual uint32_t type_persist_id() const override
        {
            if (!this->persist_id)
            {
                this->persist_id = ff::hash<std::string_view>()(this->type_name());
            }

            return this->persist_id;
        }

    private:
        uint32_t persist_id;
    };
}
