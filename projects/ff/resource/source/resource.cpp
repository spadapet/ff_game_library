#include "pch.h"
#include "resource.h"

static std::recursive_mutex& get_static_mutex() noexcept
{
    static std::recursive_mutex mutex;
    return mutex;
}

ff::resource::resource(std::string_view name, ff::value_ptr value)
    : name_data(name)
    , value_data(value)
    , loading_owner_data(nullptr)
{
    if (!this->value_data)
    {
        this->value_data = ff::value::create<nullptr_t>();
    }
}

std::string_view ff::resource::name() const
{
    return this->name_data;
}

ff::value_ptr ff::resource::value() const
{
    return this->value_data;
}

std::shared_ptr<ff::resource> ff::resource::new_resource() const
{
    std::shared_ptr<ff::resource> new_value;

    if (this->new_resource_data != nullptr)
    {
        std::lock_guard lock(::get_static_mutex());

        if (this->new_resource_data != nullptr)
        {
            new_value = this->new_resource_data;

            while (new_value->new_resource_data != nullptr)
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
    this->new_resource_data = new_value;
    this->loading_owner_data = nullptr;
}

void ff::resource::loading_owner(void* loading_owner_data)
{
    std::lock_guard lock(::get_static_mutex());
    assert(!this->loading_owner_data && loading_owner_data);
    this->loading_owner_data = loading_owner_data;
}

void* ff::resource::loading_owner()
{
    std::lock_guard lock(::get_static_mutex());
    return this->loading_owner_data;
}
