#pragma once

namespace ff
{
    class resource;

    class resource_object_provider
    {
    public:
        virtual ~resource_object_provider() = default;

        virtual std::shared_ptr<ff::resource> get_resource_object(std::string_view name) = 0;
        virtual std::vector<std::string_view> resource_object_names() const = 0;
    };

    class resource_object_loader : public resource_object_provider
    {
    public:
        virtual ~resource_object_loader() = default;

        virtual void flush_all_resources() = 0;
        virtual ff::co_task<> flush_all_resources_async() = 0;
    };
}
