#include "pch.h"
#include "resource.h"

static std::recursive_mutex& get_static_mutex() noexcept
{
    static std::recursive_mutex mutex;
    return mutex;
}

ff::resource::resource(std::string_view name, ff::value_ptr value, resource_object_loader* loading_owner)
    : name_(name)
    , value_(value ? value : ff::value::create<nullptr_t>())
    , loading_owner_(loading_owner)
{}

std::string_view ff::resource::name() const
{
    return this->name_;
}

ff::value_ptr ff::resource::value() const
{
    return this->value_;
}

std::shared_ptr<ff::resource> ff::resource::new_resource() const
{
    std::shared_ptr<ff::resource> new_value;

    if (this->new_resource_ != nullptr)
    {
        std::scoped_lock lock(::get_static_mutex());

        if (this->new_resource_ != nullptr)
        {
            new_value = this->new_resource_;

            while (new_value->new_resource_ != nullptr)
            {
                new_value = new_value->new_resource();
            }
        }
    }

    return new_value;
}

void ff::resource::new_resource(const std::shared_ptr<resource>& new_value)
{
    assert(new_value);

    std::scoped_lock lock(::get_static_mutex());
    this->new_resource_ = new_value;
    this->loading_owner_ = nullptr;
}

ff::resource_object_loader* ff::resource::loading_owner()
{
    //std::scoped_lock lock(::get_static_mutex());
    return this->loading_owner_;
}
