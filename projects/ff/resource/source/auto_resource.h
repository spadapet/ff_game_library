#pragma once

#include "resource.h"
#include "resource_object_provider.h"

namespace ff
{
    class auto_resource_value
    {
    public:
        auto_resource_value(const std::shared_ptr<ff::resource>& resource)
            : resource_(resource)
        {}

        auto_resource_value(auto_resource_value&& other) noexcept = default;
        auto_resource_value(const auto_resource_value& other) = default;

        auto_resource_value& operator=(auto_resource_value&& other) noexcept = default;
        auto_resource_value& operator=(const auto_resource_value & other) = default;

        const std::shared_ptr<ff::resource>& resource()
        {
            ff::resource_object_loader* loader = this->resource_->loading_owner();
            std::shared_ptr<ff::resource> new_resource = loader ? loader->flush_resource(this->resource_) : this->resource_->new_resource();
            return new_resource ? (this->resource_ = new_resource) : this->resource_;
        }

        ff::value_ptr value()
        {
            return this->resource()->value();
        }

    private:
        std::shared_ptr<ff::resource> resource_;
    };

    template<class T>
    class auto_resource : private auto_resource_value
    {
    public:
        using auto_resource_value::auto_resource_value;
        using auto_resource_value::operator=;
        using auto_resource_value::resource;

        const std::shared_ptr<T>& object()
        {
            ff::value_ptr value = this->resource()->value();
            if (value != this->resource_value)
            {
                auto object = value->convert_or_default<ff::resource_object_base>()->get<ff::resource_object_base>();
                this->resource_object = std::dynamic_pointer_cast<T>(object);
                this->resource_value = value;
            }

            return this->resource_object;
        }

        T* operator->()
        {
            return this->object().get();
        }

    private:
        std::shared_ptr<T> resource_object;
        ff::value_ptr resource_value;
    };
}
