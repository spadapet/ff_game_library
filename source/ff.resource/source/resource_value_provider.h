#pragma once

namespace ff
{
    class resource_value_provider
    {
    public:
        virtual ~resource_value_provider() = default;

        virtual ff::value_ptr get_resource_value(std::string_view name) const = 0;
        virtual std::string get_string_resource_value(std::string_view name) const = 0;
    };
}
