#pragma once

namespace ff
{
    class perf_measures;

    enum class perf_color
    {
        white,
        red,
        green,
        blue,
        yellow,
        cyan,
        magenta,
    };

    // Should be static
    class perf_counter
    {
    public:
        perf_counter(std::string_view name, ff::perf_color color = ff::perf_color::white);
        perf_counter(ff::perf_measures& measures, std::string_view name, ff::perf_color color = ff::perf_color::white);

        static const size_t MAX_COUNT = 256;

        ff::perf_measures& measures;
        const size_t index;
        const std::string name;
        const ff::perf_color color;

    private:
        perf_counter() = delete;
        perf_counter(const perf_counter& other) = delete;
        perf_counter(perf_counter&& other) = delete;
        perf_counter& operator=(const perf_counter& other) = delete;
        perf_counter& operator=(perf_counter&& other) = delete;
    };

    struct perf_counter_entry
    {
        const ff::perf_counter_entry* next;
        const ff::perf_counter* counter;
        int64_t ticks;
        size_t count;
        size_t level;
    };

    struct perf_counter_stats
    {
        size_t hit_total;
        size_t hit_this_second;
        size_t hit_last_second;
    };

    class perf_measures
    {
    public:
        perf_measures() = default;

        static ff::perf_measures& game();

        size_t create();
        bool start(const ff::perf_counter& counter);
        void end(const ff::perf_counter& counter, int64_t ticks);
        const ff::perf_counter_stats& stats(const ff::perf_counter& counter) const;
        const ff::perf_counter_entry* first() const;

        void reset(double absolute_seconds, bool enabled);
        double absolute_seconds() const;
        double delta_seconds() const;
        bool enabled() const;
        void enabled(bool value);

    private:
        perf_measures(const perf_measures& other) = delete;
        perf_measures(perf_measures&& other) = delete;
        perf_measures& operator=(const perf_measures& other) = delete;
        perf_measures& operator=(perf_measures&& other) = delete;

        std::array<ff::perf_counter_entry, ff::perf_counter::MAX_COUNT> entries{};
        std::array<ff::perf_counter_stats, ff::perf_counter::MAX_COUNT> stats_{};
        const ff::perf_counter_entry* first_entry{};
        ff::perf_counter_entry* last_entry{};
        double last_whole_seconds{};
        double last_absolute_seconds{};
        double last_delta_seconds{};
        size_t level{};
        size_t counters{};
        bool enabled_{ PROFILE_APP };
    };

    // Put in a method to measure the current block
    class perf_timer
    {
    public:
        perf_timer(const ff::perf_counter& counter);
        ~perf_timer();

    private:
        perf_timer() = delete;
        perf_timer(const perf_timer& other) = delete;
        perf_timer(perf_timer&& other) = delete;
        perf_timer& operator=(const perf_timer& other) = delete;
        perf_timer& operator=(perf_timer&& other) = delete;

#if PROFILE_APP
        const ff::perf_counter* counter;
        int64_t start;
#endif
    };
}
