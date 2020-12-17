#pragma once

namespace ff
{
    class resource
    {
    public:
        resource(std::string_view name, ff::value_ptr value);
        resource(resource&& other) noexcept = default;
        resource(const resource& other) = delete;

        resource& operator=(resource&& other) noexcept = default;
        resource& operator=(const resource & other) = delete;

        std::string_view name() const;
        ff::value_ptr value() const;
        std::shared_ptr<resource> new_resource() const;

        void new_resource(const std::shared_ptr<resource>& new_value);
        void loading_owner(void* loading_owner_data);
        void* loading_owner();

    private:
        std::string name_data;
        ff::value_ptr value_data;
        std::shared_ptr<resource> new_resource_data;
        void* loading_owner_data;
    };
}
