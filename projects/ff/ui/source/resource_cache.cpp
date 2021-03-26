#include "pch.h"
#include "resource_cache.h"

static const size_t MAX_COUNTER = 60;

void ff::internal::ui::resource_cache::advance()
{
    using iter_t = std::unordered_map<std::string_view, entry_t>::const_iterator;
    ff::stack_vector<iter_t, 32> entries_to_delete;

    for (auto i = this->cache.begin(); i != this->cache.end(); i++)
    {
        entry_t& entry = i->second;
        if (++entry.counter >= ::MAX_COUNTER)
        {
            entries_to_delete.push_back(i);
        }
    }

    for (auto i : entries_to_delete)
    {
        this->cache.erase(i);
    }
}

std::shared_ptr<ff::resource> ff::internal::ui::resource_cache::get_resource_object(std::string_view name)
{
    auto i = this->cache.find(name);
    if (i == this->cache.cend())
    {
        auto value = ff::global_resources::get(name);
        entry_t entry{ std::make_unique<std::string>(name), value, 0 };
        std::string_view key = *entry.name;
        i = this->cache.try_emplace(key, std::move(entry)).first;
    }

    entry_t& entry = i->second;
    entry.counter = 0;
    return entry.resource;
}

std::vector<std::string_view> ff::internal::ui::resource_cache::resource_object_names() const
{
    return ff::global_resources::get()->resource_object_names();
}
