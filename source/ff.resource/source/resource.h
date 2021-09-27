#pragma once

namespace ff
{
    class resource_object_loader;

    class resource
    {
    public:
        resource(std::string_view name, ff::value_ptr value, resource_object_loader* loading_owner = nullptr);
        resource(resource&& other) noexcept = default;
        resource(const resource& other) = delete;

        resource& operator=(resource&& other) noexcept = default;
        resource& operator=(const resource& other) = delete;

        std::string_view name() const;
        ff::value_ptr value() const;
        std::shared_ptr<resource> new_resource() const;
        void new_resource(const std::shared_ptr<resource>& new_value);
        resource_object_loader* loading_owner();

    private:
        std::string name_;
        ff::value_ptr value_;
        std::shared_ptr<resource> new_resource_;
        std::atomic<resource_object_loader*> loading_owner_;
    };
}
