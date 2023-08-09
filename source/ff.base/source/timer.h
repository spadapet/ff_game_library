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

        double time_scale() const;
        void time_scale(double scale);

        static int64_t current_raw_time();
        static int64_t raw_frequency();
        static double raw_frequency_double();
        static double seconds_between_raw(int64_t start, int64_t end);

    private:
        void reset(int64_t cur_time);

        int64_t reset_time;
        int64_t start_time;
        int64_t cur_time;

        double time_scale_;
        double start_seconds;
        double seconds_;
        double clock_seconds_;
        double pass_seconds;
    };
}
