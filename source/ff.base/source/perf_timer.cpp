#include "pch.h"
#include "assert.h"
#include "constants.h"
#include "perf_timer.h"

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

int64_t ff::perf_measures::now_ticks()
{
    return __rdtsc();
}

size_t ff::perf_measures::create()
{
    assert_msg(this->counters < ff::perf_counter::MAX_COUNT, "Too many perf_counters are registered!");
    return this->counters++;
}

void ff::perf_measures::start(const ff::perf_counter& counter)
{
    ff::perf_measures::perf_counter_stats& stats = this->stats[counter.index];
    stats.hit_total++;
    stats.hit_floor_second++;
    stats.hit_round_second++;

    ff::perf_measures::perf_counter_entry& entry = this->entries[counter.index];
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
}

void ff::perf_measures::end(const ff::perf_counter& counter, int64_t ticks)
{
    ff::perf_measures::perf_counter_entry& entry = this->entries[counter.index];
    assert(entry.counter == &counter);
    entry.ticks += ticks;
    this->level--;
}

int64_t ff::perf_measures::reset(double absolute_seconds, ff::perf_results* results, bool get_timer_results, int64_t override_start_ticks)
{
    const int64_t now_ticks = override_start_ticks ? override_start_ticks : ff::perf_measures::now_ticks();
    const int64_t delta_ticks = now_ticks - this->last_ticks;
    const double delta_seconds = absolute_seconds - this->last_absolute_seconds;
    this->last_ticks = now_ticks;
    this->last_absolute_seconds = absolute_seconds;

    if (ff::constants::profile_build)
    {
        // Update twice per second
        double cur_floor_seconds = std::floor(absolute_seconds);
        double cur_round_seconds = std::round(absolute_seconds);

        if (cur_floor_seconds > this->last_floor_seconds)
        {
            this->last_floor_seconds = cur_floor_seconds;

            for (size_t i = 0; i < this->counters; i++)
            {
                ff::perf_measures::perf_counter_stats& stats = this->stats[i];
                stats.hit_per_second = stats.hit_floor_second;
                stats.hit_floor_second = 0;
            }
        }

        if (cur_round_seconds > this->last_round_seconds)
        {
            this->last_round_seconds = cur_round_seconds;

            for (size_t i = 0; i < this->counters; i++)
            {
                ff::perf_measures::perf_counter_stats& stats = this->stats[i];
                stats.hit_per_second = stats.hit_round_second;
                stats.hit_round_second = 0;
            }
        }
    }

    if (results)
    {
        results->absolute_seconds = absolute_seconds;
        results->delta_seconds = delta_seconds;
        results->delta_ticks = delta_ticks;
        results->counter_infos.clear();
        results->counter_infos.reserve(this->counters);

        if (get_timer_results)
        {
            for (const ff::perf_measures::perf_counter_entry* entry = this->first_entry; entry; entry = entry->next)
            {
                const ff::perf_measures::perf_counter_stats& stats = this->stats[entry->counter->index];

                ff::perf_results::counter_info info;
                info.counter = entry->counter;
                info.ticks = entry->ticks;
                info.level = entry->level;
                info.hit_total = stats.hit_total;
                info.hit_last_frame = entry->count;
                info.hit_per_second = stats.hit_per_second;

                results->counter_infos.push_back(info);
            }
        }
    }

    this->first_entry = nullptr;
    this->last_entry = nullptr;
    this->level = 0;
    std::memset(this->entries.data(), 0, this->counters * sizeof(ff::perf_measures::perf_counter_entry));

    return now_ticks;
}

#if PROFILE_APP

ff::perf_timer::perf_timer(const ff::perf_counter& counter)
    : ff::perf_timer(counter, ff::perf_measures::now_ticks())
{}

ff::perf_timer::perf_timer(const ff::perf_counter& counter, int64_t start_ticks)
    : counter(counter)
    , start(start_ticks)
{
    counter.measures.start(counter);
}

ff::perf_timer::~perf_timer()
{
    const int64_t end = ff::perf_measures::now_ticks();
    this->counter.measures.end(this->counter, (end - this->start) * (end > this->start));
}

#else

ff::perf_timer::perf_timer(const ff::perf_counter& counter) {}
ff::perf_timer::perf_timer(const ff::perf_counter& counter, int64_t ticks) {}
ff::perf_timer::perf_timer(ff::perf_timer&& other) noexcept {}
ff::perf_timer& ff::perf_timer::operator=(ff::perf_timer&& other) noexcept {}
ff::perf_timer::~perf_timer() {}

#endif
