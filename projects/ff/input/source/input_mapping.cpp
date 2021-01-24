#include "pch.h"
#include "input_mapping.h"

ff::input_mapping_def::~input_mapping_def()
{}

bool ff::input_event::started() const
{
    return this->count == 1;
}

bool ff::input_event::repeated() const
{
    return this->count > 1;
}

bool ff::input_event::stopped() const
{
    return this->count == 0;
}

ff::input_event_provider::input_event_provider(const input_mapping_def& mapping, std::vector<input_vk const*>&& devices)
    : devices(std::move(devices))
{
    this->event_id_to_progress.reserve(mapping.events().size());
    this->value_id_to_vk.reserve(mapping.values().size());

    for (const ff::input_event_def& event_def : mapping.events())
    {
        input_event_progress event_progress{};
        static_cast<ff::input_event_def&>(event_progress) = event_def;
        this->event_id_to_progress.insert(std::make_pair(event_def.event_id, event_progress));
    }

    for (const ff::input_value_def& value_def : mapping.values())
    {
        this->value_id_to_vk.insert(std::make_pair(value_def.value_id, value_def.vk));
    }
}

void ff::input_event_provider::advance(double delta_time)
{}

const std::vector<ff::input_event>& ff::input_event_provider::events() const
{
    return this->events_;
}

float ff::input_event_provider::event_progress(size_t event_id) const
{
    return 0.0f;
}

bool ff::input_event_provider::event_started(size_t event_id) const
{
    return false;
}

bool ff::input_event_provider::digital_value(size_t value_id) const
{
    return false;
}

float ff::input_event_provider::analog_value(size_t value_id) const
{
    return 0.0f;
}

bool ff::input_event_provider::get_digital_value(int vk, int& press_count) const
{
    return false;
}

float ff::input_event_provider::get_analog_value(int vk) const
{
    return 0.0f;
}

void ff::input_event_provider::push_start_event(input_event_progress& event)
{}

void ff::input_event_provider::push_stop_event(input_event_progress& event)
{}

ff::input_mapping_o::input_mapping_o(std::vector<input_event_def>&& events, std::vector<input_value_def>&& values)
    : events_(std::move(events))
    , values_(std::move(values))
{}

const std::vector<ff::input_event_def>& ff::input_mapping_o::events() const
{
    return this->events_;
}

const std::vector<ff::input_value_def>& ff::input_mapping_o::values() const
{
    return this->values_;
}

std::shared_ptr<ff::resource_object_base> ff::internal::input_mapping_factory::load_from_source(const ff::dict& dict, ff::resource_load_context& context) const
{
    // TODO:
    return nullptr;
}

std::shared_ptr<ff::resource_object_base> ff::internal::input_mapping_factory::load_from_cache(const ff::dict& dict) const
{
    // TODO:
    return nullptr;
}
