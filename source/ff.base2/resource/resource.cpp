#include "pch.h"
#include "data_value/null_v.h"
#include "resource/resource.h"

ff::resource::resource(std::string_view name)
    : name_(name)
    , value_(ff::value::create<nullptr_t>())
    , finalized_event(std::make_shared<ff::win_event>())
{}

ff::resource::resource(std::string_view name, ff::value_ptr value)
    : name_(name)
    , value_(value ? value : ff::value::create<nullptr_t>())
    , finalized_flag(true)
{}

std::string_view ff::resource::name() const
{
    return this->name_;
}

ff::value_ptr ff::resource::value() const
{
    if (this->is_loading())
    {
        auto finalized_event = this->finalized_event.load();
        if (finalized_event && !finalized_event->is_set())
        {
            finalized_event->wait();
        }
    }

    return this->value_;
}

ff::co_task<ff::value_ptr> ff::resource::value_async() const
{
    if (this->is_loading())
    {
        auto finalized_event = this->finalized_event.load();
        if (finalized_event && !finalized_event->is_set())
        {
            co_await *finalized_event;
        }
    }

    co_return this->value_;
}

bool ff::resource::is_loading() const
{
    return !this->finalized_flag;
}

void ff::resource::finalize_value(ff::value_ptr value)
{
    auto finalized_event = this->finalized_event.load();
    assert(!this->finalized_flag && finalized_event && !finalized_event->is_set());

    if (finalized_event)
    {
        this->value_ = value ? value : ff::value::create<nullptr_t>();
        finalized_event->set();
        this->finalized_event.store(nullptr);
    }

    this->finalized_flag = true;
}

void ff::resource::new_resource(std::shared_ptr<ff::resource> value)
{
    assert(!this->new_resource_flag && !this->new_resource_.load());
    this->new_resource_.store(value);
    this->new_resource_flag = true;
}

std::shared_ptr<ff::resource> ff::resource::new_resource() const
{
    if (this->new_resource_flag)
    {
        std::shared_ptr<ff::resource> result = this->new_resource_.load();
        if (result)
        {
            std::shared_ptr<ff::resource> result2 = result->new_resource();
            return result2 ? result2 : result;
        }
    }

    return {};
}
