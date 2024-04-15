#pragma once

#include "resource.h"
#include "resource_object_base.h"
#include "resource_object_provider.h"

namespace ff
{
    class auto_resource_value
    {
    public:
        auto_resource_value() = default;
        auto_resource_value(auto_resource_value&& other) noexcept = default;
        auto_resource_value(const auto_resource_value& other) = default;
        auto_resource_value(const std::shared_ptr<ff::resource>& resource);
        auto_resource_value(std::string_view resource_name);

        auto_resource_value& operator=(auto_resource_value&& other) noexcept = default;
        auto_resource_value& operator=(const auto_resource_value& other) = default;
        auto_resource_value& operator=(const std::shared_ptr<ff::resource>& resource);
        auto_resource_value& operator=(std::string_view resource_name);

        const std::shared_ptr<ff::resource>& resource() const;
        const std::shared_ptr<ff::resource>& latest_resource();
        ff::resource* operator->();

    private:
        std::shared_ptr<ff::resource> resource_;
    };

    template<class T>
    class auto_resource : private ff::auto_resource_value
    {
    public:
        using ff::auto_resource_value::resource;

        auto_resource() = default;
        auto_resource(auto_resource&& other) noexcept = default;
        auto_resource(const auto_resource& other) = default;
        auto_resource& operator=(auto_resource&& other) noexcept = default;
        auto_resource& operator=(const auto_resource& other) = default;

        auto_resource(const std::shared_ptr<ff::resource>& resource)
            : ff::auto_resource_value(resource)
        {}

        auto_resource(std::string_view resource_name)
            : ff::auto_resource_value(resource_name)
        {}

        auto_resource& operator=(const ff::auto_resource_value& resource)
        {
            *this = resource.resource();
            return *this;
        }

        auto_resource& operator=(const std::shared_ptr<ff::resource>& resource)
        {
            if (this->resource() != resource)
            {
                this->resource_object.reset();
                ff::auto_resource_value::operator=(resource);
            }

            return *this;
        }

        auto_resource& operator=(std::string_view resource_name)
        {
            *this = ff::auto_resource_value(resource_name);
            return *this;
        }

        const std::shared_ptr<T>& object()
        {
            std::shared_ptr<ff::resource> resource = this->latest_resource();
            if (this->resource_for_object != resource)
            {
                ff::value_ptr value = resource ? resource->value() : ff::value::create<nullptr_t>();
                this->create_resource_object(resource, value);
            }

            return this->resource_object;
        }

        ff::co_task<std::shared_ptr<T>> object_async()
        {
            std::shared_ptr<ff::resource> resource = this->latest_resource();
            if (this->resource_for_object != resource)
            {
                ff::value_ptr value = resource ? co_await resource->value_async() : ff::value::create<nullptr_t>();
                this->create_resource_object(resource, value);
            }

            co_return this->resource_object;
        }

        T* operator->()
        {
            return this->object().get();
        }

    private:
        void create_resource_object(const std::shared_ptr<ff::resource>& resource, ff::value_ptr value)
        {
            std::shared_ptr<ff::resource_object_base> object = value->convert_or_default<ff::resource_object_base>()->get<ff::resource_object_base>();
            this->resource_for_object = resource;
            this->resource_object = std::dynamic_pointer_cast<T>(object);
            assert(this->resource_object || value->is_type<nullptr_t>());
        }

        std::shared_ptr<ff::resource> resource_for_object;
        std::shared_ptr<T> resource_object;
    };

    template<class T>
    static std::shared_ptr<T> get_resource(std::string_view name)
    {
        ff::auto_resource<T> res(name);
        return res.object();
    }

    template<class T>
    static std::shared_ptr<T> get_resource(ff::resource_object_provider& provider, std::string_view name)
    {
        ff::auto_resource<T> res(provider.get_resource_object(name));
        return res.object();
    }

    template<class T>
    static ff::co_task<std::shared_ptr<T>> get_resource_async(std::string_view name)
    {
        ff::auto_resource<T> res(name);
        return res.object_async();
    }

    template<class T>
    static ff::co_task<std::shared_ptr<T>> get_resource_async(ff::resource_object_provider& provider, std::string_view name)
    {
        ff::auto_resource<T> res(provider.get_resource_object(name));
        return res.object_async();
    }
}
