#pragma once

#include "../value/value.h"
#include "../value/value_type_base.h"

namespace ff
{
    class saved_data_base;
}

namespace ff::type
{
    class saved_data_v : public ff::value
    {
    public:
        saved_data_v(std::shared_ptr<ff::saved_data_base>&& value);
        saved_data_v(const std::shared_ptr<ff::saved_data_base>& value);

        const std::shared_ptr<ff::saved_data_base>& get() const;
        static ff::value* get_static_value(std::shared_ptr<ff::saved_data_base>&& value);
        static ff::value* get_static_value(const std::shared_ptr<ff::saved_data_base>& value);
        static ff::value* get_static_default_value();

    private:
        std::shared_ptr<ff::saved_data_base> value;
    };

    template<>
    struct value_traits<ff::saved_data_base> : public value_derived_traits<ff::type::saved_data_v>
    {};

    class saved_data_type : public ff::value_type_base<ff::type::saved_data_v>
    {
    public:
        using value_type_base::value_type_base;

        virtual value_ptr try_convert_to(const value* val, std::type_index type) const override;
        virtual value_ptr load(reader_base& reader) const override;
        virtual bool save(const value* val, writer_base& writer) const override;
        virtual void print(const value* val, std::ostream& output) const override;

        static value_ptr load_helper(reader_base& reader);
    };
}
