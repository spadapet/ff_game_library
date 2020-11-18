#pragma once

namespace ff
{
    /// <summary>
    /// High accuracy timer
    /// </summary>
    /// <remarks>
    /// Measures time and ticks per second. The time scale can be modified on the fly.
    /// </remarks>
    class timer
    {
    public:
        timer();
        ~timer();

        double tick(double forcedOffset = -1.0); // Update the time
        void reset(); // Start counting seconds from zero

        double get_seconds() const; // seconds passed since the last Reset, affected by SetTimeScale
        double get_tick_seconds() const; // seconds passed since the last Tick
        double get_clock_seconds() const; // seconds passed since the last Reset, not affected by SetTimeScale
        size_t get_tick_count() const; // number of times Tick() was called
        size_t get_ticks_per_second() const; // number of times Tick() was called during the last second

        double get_time_scale() const;
        void set_time_scale(double scale);

        int64_t get_last_tick_raw_time() const;
        static int64_t get_current_raw_time();
        static int64_t get_raw_frequency_static();
        int64_t get_raw_frequency() const;
        double get_raw_frequency_double() const;

        void store_last_tick_raw_time();
        int64_t get_last_tick_stored_raw_time(); // updates the stored start time too
        int64_t get_current_stored_raw_time(); // updates the stored start time too

    private:
        int64_t reset_time;
        int64_t start_time;
        int64_t cur_time;
        int64_t stored_time;
        int64_t frequency;

        size_t tick_count;
        size_t tps_second;
        size_t tps_cur_second;
        size_t tps_count;
        size_t tps;

        double time_scale;
        double start_seconds;
        double seconds;
        double clock_seconds;
        double frequency_double;
        double pass_seconds;
    };
}
