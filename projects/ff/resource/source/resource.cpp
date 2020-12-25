#include "pch.h"
#include "resource.h"

static std::recursive_mutex& get_static_mutex() noexcept
{
    static std::recursive_mutex mutex;
    return mutex;
}

ff::resource::resource(std::string_view name, ff::value_ptr value)
    : name_(name)
    , value_(value)
    , loading_owner_(nullptr)
{
    if (!this->value_)
    {
        this->value_ = ff::value::create<nullptr_t>();
    }
}

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
        std::lock_guard lock(::get_static_mutex());

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

    std::lock_guard lock(::get_static_mutex());
    this->new_resource_ = new_value;
    this->loading_owner_ = nullptr;
}

void ff::resource::loading_owner(resource_object_loader* loading_owner_)
{
    std::lock_guard lock(::get_static_mutex());
    assert(!this->loading_owner_ && loading_owner_);
    this->loading_owner_ = loading_owner_;
}

ff::resource_object_loader* ff::resource::loading_owner()
{
    std::lock_guard lock(::get_static_mutex());
    return this->loading_owner_;
}
