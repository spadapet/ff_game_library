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
        auto_resource_value(const std::shared_ptr<ff::resource>& resource);
        auto_resource_value(std::string_view resource_name);
        auto_resource_value(auto_resource_value&& other) noexcept = default;
        auto_resource_value(const auto_resource_value& other) = default;

        auto_resource_value& operator=(auto_resource_value&& other) noexcept = default;
        auto_resource_value& operator=(const auto_resource_value& other) = default;
        auto_resource_value& operator=(const std::shared_ptr<ff::resource>& resource);
        auto_resource_value& operator=(std::string_view resource_name);

        bool valid() const;
        const std::shared_ptr<ff::resource>& resource() const;
        ff::value_ptr value(bool force = true) const;

    private:
        std::shared_ptr<ff::resource> resource_;
    };

    template<class T>
    class auto_resource : private auto_resource_value
    {
    public:
        using auto_resource_value::auto_resource_value;
        using auto_resource_value::operator=;
        using auto_resource_value::valid;
        using auto_resource_value::resource;

        auto_resource(const auto_resource_value& other)
            : auto_resource_value(other)
        {}

        auto_resource(auto_resource_value&& other) noexcept
            : auto_resource_value(std::move(other))
        {}

        const std::shared_ptr<T>& object()
        {
            if (!this->cached_object)
            {
                this->object_async().wait();
                assert(this->cached_object);
            }

            return this->resource_object;
        }

        ff::co_task<std::shared_ptr<T>> object_async()
        {
            if (!this->cached_object)
            {
                std::shared_ptr<ff::resource_object_base> object;
                if (this->valid())
                {
                    ff::value_ptr value = co_await this->resource()->value_async();
                    object = value->convert_or_default<ff::resource_object_base>()->get<ff::resource_object_base>();
                }

                this->resource_object = std::dynamic_pointer_cast<T>(object);
                assert(this->resource_object);
                this->cached_object = true;
            }

            co_return this->resource_object;
        }

        T* operator->()
        {
            return this->object().get();
        }

    private:
        std::shared_ptr<T> resource_object;
        bool cached_object{};
    };
}
