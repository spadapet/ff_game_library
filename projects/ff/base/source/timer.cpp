#include "pch.h"
#include "timer.h"

ff::timer::timer()
    : tick_count(0)
    , tps_second(0)
    , tps_cur_second(0)
    , tps_count(0)
    , tps(0)
    , time_scale(1)
    , start_seconds(0)
    , seconds(0)
    , clock_seconds(0)
    , frequency_double(0)
    , pass_seconds(0)
{
    ::QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&this->frequency));

    this->reset_time = this->get_current_raw_time();
    this->start_time = this->reset_time;
    this->cur_time = this->reset_time;
    this->stored_time = this->reset_time;
    this->frequency_double = static_cast<double>(this->frequency);
}

ff::timer::~timer()
{
}

double ff::timer::tick(double forcedOffset)
{
    double oldTime = this->seconds;

    this->cur_time = this->get_current_raw_time();
    this->clock_seconds = (this->cur_time - this->reset_time) / this->frequency_double;

    if (forcedOffset < 0)
    {
        this->seconds = this->start_seconds + (this->time_scale * (this->cur_time - this->start_time) / this->frequency_double);
        this->pass_seconds = this->seconds - oldTime;
    }
    else
    {
        this->seconds += forcedOffset;
        this->start_seconds = this->seconds;
        this->start_time = this->cur_time;
        this->pass_seconds = forcedOffset;
    }

    if (this->pass_seconds < 0)
    {
        // something weird happened (sleep mode? processor switch?)
        this->reset();
    }

    // ticks per second stuff
    this->tick_count++;
    this->tps_count++;
    this->tps_cur_second = static_cast<size_t>(this->seconds);

    if (this->tps_cur_second > this->tps_second)
    {
        this->tps = this->tps_count / (this->tps_cur_second - this->tps_second);
        this->tps_count = 0;
        this->tps_second = this->tps_cur_second;
    }

    return this->pass_seconds;
}

void ff::timer::reset()
{
    this->start_time = this->get_current_raw_time();
    this->cur_time = this->start_time;
    this->stored_time = this->start_time;
    this->tick_count = 0;
    this->tps_second = 0;
    this->tps_count = 0;
    this->tps_cur_second = 0;
    this->tps = 0;
    this->start_seconds = 0;
    this->seconds = 0;
    this->pass_seconds = 0;

    // _timeScale stays the same
}

double ff::timer::get_seconds() const
{
    return this->seconds;
}

double ff::timer::get_tick_seconds() const
{
    return this->pass_seconds;
}

double ff::timer::get_clock_seconds() const
{
    return this->clock_seconds;
}

size_t ff::timer::get_tick_count() const
{
    return this->tick_count;
}

size_t ff::timer::get_ticks_per_second() const
{
    return this->tps;
}

double ff::timer::get_time_scale() const
{
    return this->time_scale;
}

void ff::timer::set_time_scale(double scale)
{
    if (scale != this->time_scale)
    {
        this->time_scale = scale;
        this->start_seconds = this->seconds;
        this->start_time = this->cur_time;
    }
}

int64_t ff::timer::get_last_tick_raw_time() const
{
    return this->cur_time;
}

int64_t ff::timer::get_current_raw_time()
{
    int64_t curTime;
    ::QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&curTime));
    return curTime;
}

int64_t ff::timer::get_raw_frequency_static()
{
    int64_t value;
    ::QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&value));
    return value;
}

int64_t ff::timer::get_raw_frequency() const
{
    return this->frequency;
}

double ff::timer::get_raw_frequency_double() const
{
    return this->frequency_double;
}

void ff::timer::store_last_tick_raw_time()
{
    this->stored_time = this->cur_time;
}

int64_t ff::timer::get_last_tick_stored_raw_time()
{
    int64_t diff = this->cur_time - this->stored_time;
    this->stored_time = this->cur_time;

    return diff;
}

int64_t ff::timer::get_current_stored_raw_time()
{
    int64_t curTime = this->get_current_raw_time();
    int64_t diff = curTime - this->stored_time;
    this->stored_time = curTime;

    return diff;
}
