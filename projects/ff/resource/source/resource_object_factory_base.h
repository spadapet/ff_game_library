#pragma once

namespace ff
{
    class resource_load_context;
    class resource_object_base;

    class resource_object_factory_base
    {
    public:
        resource_object_factory_base(std::string_view name);
        virtual ~resource_object_factory_base() = 0;

        std::string_view name() const;

        virtual std::type_index type_index() const = 0;
        virtual std::shared_ptr<resource_object_base> load_from_source(const ff::dict& dict, resource_load_context& context) const = 0;
        virtual std::shared_ptr<resource_object_base> load_from_cache(const ff::dict& dict) const = 0;

    private:
        std::string name_;
    };

    template<class T>
    class resource_object_factory : public resource_object_factory_base
    {
    public:
        using resource_object_factory_base::resource_object_factory_base;

        virtual std::type_index type_index() const override
        {
            return typeid(T);
        }
    };
}
