#pragma once

namespace ff
{
    class resource_object_base;
}

namespace ff::type
{
    class resource_object_v : public ff::value
    {
    public:
        resource_object_v(std::shared_ptr<ff::resource_object_base>&& value);
        resource_object_v(const std::shared_ptr<ff::resource_object_base>& value);

        const std::shared_ptr<ff::resource_object_base>& get() const;
        static ff::value* get_static_value(const std::shared_ptr<ff::resource_object_base>& value);
        static ff::value* get_static_default_value();

    private:
        std::shared_ptr<ff::resource_object_base> value;
    };

    template<>
    struct value_traits<ff::resource_object_base> : public value_derived_traits<ff::type::resource_object_v>
    {};

    class resource_object_type : public ff::internal::value_type_base<ff::type::resource_object_v>
    {
    public:
        using value_type_base::value_type_base;

        virtual value_ptr try_convert_to(const value* val, std::type_index type) const override;
        virtual value_ptr load(reader_base& reader) const override;
        virtual bool save(const value* val, writer_base& writer) const override;
        virtual void print(const value* val, std::ostream& output) const override;
    };
}
