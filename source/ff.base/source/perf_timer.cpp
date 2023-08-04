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

ff::perf_measures::perf_measures()
{
    this->reset();
}

ff::perf_measures& ff::perf_measures::game()
{
    return ::perf_measures_game;
}

size_t ff::perf_measures::create()
{
    assert_msg(this->counters < ff::perf_counter::MAX_COUNT, "Too many perf_counters are registered!");
    return this->counters++;
}

void ff::perf_measures::start(const ff::perf_counter& counter)
{
    if (!this->disabled)
    {
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

            if (this->last_entry)
            {
                this->last_entry->next = &entry;
            }

            this->last_entry = &entry;
        }

        this->level++;
    }
}

void ff::perf_measures::end(const ff::perf_counter& counter, int64_t ticks)
{
    if (!this->disabled)
    {
    }
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
    if (value == this->disabled)
    {
        this->disabled = !value;

        if (!this->disabled)
        {
            this->reset();
        }
    }
}

#if PROFILE_APP

ff::perf_timer::perf_timer(const ff::perf_counter& counter)
    : counter(counter)
    , start(__rdtsc())
{
    this->counter.measures.start(this->counter);
}

ff::perf_timer::~perf_timer()
{
    int64_t end = __rdtsc();
    this->counter.measures.end(this->counter, (end - this->start) * (end > this->start));
}

#else

ff::perf_timer::perf_timer(const ff::perf_counter& counter)
{}

ff::perf_timer::perf_timer(const ff::perf_counter& counter, ff::perf_measures& measures)
{}

ff::perf_timer::~perf_timer()
{}

#endif
