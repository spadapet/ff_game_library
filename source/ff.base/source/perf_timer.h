#pragma once

namespace ff
{
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
    struct perf_counter
    {
        perf_counter(std::string_view name, ff::perf_color color = ff::perf_color::white);
        ~perf_counter();

        static const size_t MAX_COUNT = 256;

        const size_t index;
        const std::string name;
        const ff::perf_color color;
    };

    struct perf_counter_entry
    {
        const ff::perf_counter* counter;
        const ff::perf_counter_entry* next;
        int64_t ticks;
        size_t count;
        size_t level;
    };

    class perf_timer
    {
    public:
        perf_timer(const ff::perf_counter& counter);
        ~perf_timer();

    private:
        const ff::perf_counter& counter;
    };

    class perf_measures
    {
    public:
        void start(const ff::perf_counter& counter);
        void end(const ff::perf_counter& counter);
        const ff::perf_counter_entry* first() const;
        void reset();
        bool enabled() const;
        void enabled(bool value);

    private:
        std::array<ff::perf_counter_entry, ff::perf_counter::MAX_COUNT> entries;
        const ff::perf_counter_entry* first_entry;
        const ff::perf_counter_entry* last_entry;
    };
}
