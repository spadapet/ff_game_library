#pragma once

#include "value.h"
#include "value_type_base.h"

namespace ff
{
    class data_base;
    enum class saved_data_type;
}

namespace ff::type
{
    class data_v : public ff::value
    {
    public:
        data_v(std::shared_ptr<ff::data_base>&& value, ff::saved_data_type saved_data_type);
        data_v(const std::shared_ptr<ff::data_base>& value, ff::saved_data_type saved_data_type);

        const std::shared_ptr<ff::data_base>& get() const;
        ff::saved_data_type saved_data_type() const;
        static ff::value* get_static_value(const std::shared_ptr<ff::data_base>& value, ff::saved_data_type saved_data_type);
        static ff::value* get_static_default_value();

    private:
        std::shared_ptr<ff::data_base> value;
        ff::saved_data_type saved_data_type_;
    };

    template<>
    struct value_traits<ff::data_base> : public value_derived_traits<ff::type::data_v>
    {};

    class data_type : public ff::internal::value_type_base<ff::type::data_v>
    {
    public:
        using value_type_base::value_type_base;

        virtual value_ptr try_convert_to(const value* val, std::type_index type) const override;
        virtual value_ptr load(reader_base& reader) const override;
        virtual bool save(const value* val, writer_base& writer) const override;
        virtual void print(const value* val, std::ostream& output) const override;
    };
}
