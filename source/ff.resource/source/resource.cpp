#include "pch.h"
#include "resource.h"

ff::resource::resource(std::string_view name)
    : name_(name)
    , value_(ff::value::create<nullptr_t>())
    , finalized_event(std::make_shared<ff::win_event>())
{}

ff::resource::resource(std::string_view name, ff::value_ptr value)
    : name_(name)
    , value_(value ? value : ff::value::create<nullptr_t>())
    , finalized_event(!value ? std::make_shared<ff::win_event>() : std::shared_ptr<ff::win_event>())
{}

std::string_view ff::resource::name() const
{
    return this->name_;
}

ff::value_ptr ff::resource::value() const
{
    auto finalized_event = this->finalized_event.load();
    if (finalized_event && !finalized_event->is_set())
    {
        int64_t start_time = ff::timer::current_raw_time();

        finalized_event->wait();

        const double seconds = ff::timer::seconds_between_raw(start_time, ff::timer::current_raw_time());
        ff::log::write(ff::log::type::resource, "Waited (blocked): ", this->name_, " (", &std::fixed, std::setprecision(1), seconds * 1000.0, "ms)");
    }

    return this->value_;
}

ff::co_task<ff::value_ptr> ff::resource::value_async() const
{
    auto finalized_event = this->finalized_event.load();
    if (finalized_event && !finalized_event->is_set())
    {
        int64_t start_time = ff::timer::current_raw_time();
        co_await *finalized_event;

        const double seconds = ff::timer::seconds_between_raw(start_time, ff::timer::current_raw_time());
        ff::log::write(ff::log::type::resource, "Waited (async): ", this->name_, " (", &std::fixed, std::setprecision(1), seconds * 1000.0, "ms)");
    }

    assert(!this->finalized_event.load());
    co_return this->value_;
}

bool ff::resource::is_loading() const
{
    auto finalized_event = this->finalized_event.load();
    return finalized_event && !finalized_event->is_set();
}

void ff::resource::finalize_value(ff::value_ptr value)
{
    auto finalized_event = this->finalized_event.load();
    assert(finalized_event && !finalized_event->is_set());

    if (finalized_event)
    {
        this->value_ = value ? value : ff::value::create<nullptr_t>();
        finalized_event->set();
        this->finalized_event.store(nullptr);
    }
}
