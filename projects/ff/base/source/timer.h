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

        double tick(double forced_offset = -1.0); // Update the time
        void reset(); // Start counting seconds from zero

        double seconds() const; // seconds passed since the last Reset, affected by SetTimeScale
        double tick_seconds() const; // seconds passed since the last Tick
        double clock_seconds() const; // seconds passed since the last Reset, not affected by SetTimeScale
        size_t tick_count() const; // number of times tick() was called
        size_t ticks_per_second() const; // number of times tick() was called during the last second

        double time_scale() const;
        void time_scale(double scale);

        int64_t last_tick_raw_time() const;
        static int64_t current_raw_time();
        static int64_t raw_frequency_static();
        static double raw_frequency_double_static();
        int64_t raw_frequency() const;
        double raw_frequency_double() const;

        void store_last_tick_raw_time();
        int64_t last_tick_stored_raw_time(); // updates the stored start time too
        int64_t current_stored_raw_time(); // updates the stored start time too

    private:
        int64_t reset_time;
        int64_t start_time;
        int64_t cur_time;
        int64_t stored_time;
        int64_t frequency;

        size_t tick_count_;
        size_t tps_second;
        size_t tps_cur_second;
        size_t tps_count;
        size_t tps;

        double time_scale_;
        double start_seconds;
        double seconds_;
        double clock_seconds_;
        double frequency_double;
        double pass_seconds;
    };
}
