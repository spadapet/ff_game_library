#include "pch.h"
#include "assert.h"
#include "perf_timer.h"
#include "stack_vector.h"

static ff::perf_measures perf_measures_game;

ff::perf_counter::perf_counter(std::string_view name, ff::perf_color color)
    : ff::perf_counter(::perf_measures_game, name, color)
{}

ff::perf_counter::perf_counter(ff::perf_measures& measures, std::string_view name, ff::perf_color color)
    : measures(measures)
    , index(measures.create())
    , name(name)
    , color(color)
{}

ff::perf_measures& ff::perf_measures::game()
{
    return ::perf_measures_game;
}

size_t ff::perf_measures::create()
{
    assert_msg(this->counters < ff::perf_counter::MAX_COUNT, "Too many perf_counters are registered!");
    return this->counters++;
}

bool ff::perf_measures::start(const ff::perf_counter& counter)
{
    if (this->disabled || counter.index >= this->entries.size())
    {
        return false;
    }

    ff::perf_counter_entry& entry = this->entries[counter.index];
    entry.count++;

    if (!entry.counter)
    {
        // First time a counter is hit
        entry.counter = &counter;
        entry.level = this->level;

        if (!this->first_entry)
        {
            this->first_entry = &entry;
        }
        else
        {
            this->last_entry->next = &entry;
        }

        this->last_entry = &entry;
    }

    this->level++;
    return true;
}

void ff::perf_measures::end(const ff::perf_counter& counter, int64_t ticks)
{
    ff::perf_counter_entry& entry = this->entries[counter.index];
    assert(entry.counter == &counter);
    entry.ticks += ticks;
    this->level--;
}

const ff::perf_counter_entry* ff::perf_measures::first() const
{
    return this->first_entry;
}

void ff::perf_measures::reset()
{
    if (!this->disabled)
    {
        this->first_entry = nullptr;
        this->last_entry = nullptr;
        this->level = 0;
        std::memset(this->entries.data(), 0, ff::array_byte_size(this->entries));
    }
}

bool ff::perf_measures::enabled() const
{
    return !this->disabled;
}

void ff::perf_measures::enabled(bool value)
{
    this->disabled = !value;
}

#if PROFILE_APP

ff::perf_timer::perf_timer(const ff::perf_counter& counter)
    : counter(counter.measures.start(counter) ? &counter : nullptr)
    , start(__rdtsc())
{}

ff::perf_timer::~perf_timer()
{
    if (this->counter)
    {
        int64_t end = __rdtsc();
        this->counter->measures.end(*this->counter, (end - this->start) * (end > this->start));
    }
}

#else

ff::perf_timer::perf_timer(const ff::perf_counter& counter)
{}

ff::perf_timer::perf_timer(const ff::perf_counter& counter, ff::perf_measures& measures)
{}

ff::perf_timer::~perf_timer()
{}

#endif
