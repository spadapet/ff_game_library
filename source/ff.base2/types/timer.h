#pragma once

namespace ff
{
    class timer
    {
    public:
        timer();

        double tick(); // Update the time, return time since last tick
        void reset(); // Start counting seconds from zero
        double seconds() const; // seconds passed since the last reset
        double delta_seconds() const; // seconds between last ticks

        static int64_t current_raw_time();
        static int64_t raw_frequency();
        static double raw_frequency_double();
        static double seconds_between_raw(int64_t start, int64_t end);
        static double seconds_since_raw(int64_t start);

    private:
        void reset(int64_t now_time);

        int64_t reset_time;
        int64_t now_time;
        double seconds_;
        double delta_seconds_;
    };
}
