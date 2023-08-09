#include "pch.h"
#include "assert.h"
#include "perf_timer.h"
#include "stack_vector.h"

static ff::perf_measures perf_measures_game;
static const ff::perf_counter_stats empty_stats{};

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
    if (counter.index >= this->entries.size())
    {
        return false;
    }

    this->stats_[counter.index].hit_total++;
    this->stats_[counter.index].hit_this_second++;

    if (!this->enabled_)
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

const ff::perf_counter_stats& ff::perf_measures::stats(const ff::perf_counter& counter) const
{
    return counter.index < this->stats_.size() ? this->stats_[counter.index] : ::empty_stats;
}

const ff::perf_counter_entry* ff::perf_measures::first() const
{
    return this->first_entry;
}

void ff::perf_measures::reset(double absolute_seconds, bool enabled)
{
    this->enabled(enabled);
    this->last_delta_seconds = absolute_seconds - this->last_absolute_seconds;
    this->last_absolute_seconds = absolute_seconds;

    double cur_whole_seconds = std::floor(absolute_seconds);
    if (ff::constants::profile_build && cur_whole_seconds > this->last_whole_seconds)
    {
        this->last_whole_seconds = cur_whole_seconds;

        for (ff::perf_counter_stats& stats : this->stats_)
        {
            stats.hit_last_second = stats.hit_this_second;
            stats.hit_this_second = 0;
        }
    }

    if (this->enabled_)
    {
        this->first_entry = nullptr;
        this->last_entry = nullptr;
        this->level = 0;
        std::memset(this->entries.data(), 0, ff::array_byte_size(this->entries));
    }
}

double ff::perf_measures::absolute_seconds() const
{
    return this->last_absolute_seconds;
}

double ff::perf_measures::delta_seconds() const
{
    return this->last_delta_seconds;
}

bool ff::perf_measures::enabled() const
{
    return this->enabled_;
}

void ff::perf_measures::enabled(bool value)
{
    this->enabled_ = ff::constants::profile_build && value;
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

ff::perf_timer::~perf_timer()
{}

#endif
