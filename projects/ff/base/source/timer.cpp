#include "pch.h"
#include "timer.h"

ff::timer::timer()
    : tick_count_(0)
    , tps_second(0)
    , tps_cur_second(0)
    , tps_count(0)
    , tps(0)
    , time_scale_(1)
    , start_seconds(0)
    , seconds_(0)
    , clock_seconds_(0)
    , frequency_double(0)
    , pass_seconds(0)
{
    ::QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&this->frequency));

    this->reset_time = ff::timer::current_raw_time_static();
    this->start_time = this->reset_time;
    this->cur_time = this->reset_time;
    this->stored_time = this->reset_time;
    this->frequency_double = static_cast<double>(this->frequency);
}

double ff::timer::tick(double forced_offset)
{
    double oldTime = this->seconds_;

    this->cur_time = ff::timer::current_raw_time_static();
    this->clock_seconds_ = (this->cur_time - this->reset_time) / this->frequency_double;

    if (forced_offset < 0)
    {
        this->seconds_ = this->start_seconds + (this->time_scale_ * (this->cur_time - this->start_time) / this->frequency_double);
        this->pass_seconds = this->seconds_ - oldTime;
    }
    else
    {
        this->seconds_ += forced_offset;
        this->start_seconds = this->seconds_;
        this->start_time = this->cur_time;
        this->pass_seconds = forced_offset;
    }

    if (this->pass_seconds < 0)
    {
        // something weird happened (sleep mode? processor switch?)
        this->reset();
    }

    // ticks per second stuff
    this->tick_count_++;
    this->tps_count++;
    this->tps_cur_second = static_cast<size_t>(this->seconds_);

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
    this->start_time = ff::timer::current_raw_time_static();
    this->cur_time = this->start_time;
    this->stored_time = this->start_time;
    this->tick_count_ = 0;
    this->tps_second = 0;
    this->tps_count = 0;
    this->tps_cur_second = 0;
    this->tps = 0;
    this->start_seconds = 0;
    this->seconds_ = 0;
    this->pass_seconds = 0;

    // this->time_scale stays the same
}

double ff::timer::seconds() const
{
    return this->seconds_;
}

double ff::timer::tick_seconds() const
{
    return this->pass_seconds;
}

double ff::timer::clock_seconds() const
{
    return this->clock_seconds_;
}

size_t ff::timer::tick_count() const
{
    return this->tick_count_;
}

size_t ff::timer::ticks_per_second() const
{
    return this->tps;
}

double ff::timer::time_scale() const
{
    return this->time_scale_;
}

void ff::timer::time_scale(double scale)
{
    if (scale != this->time_scale_)
    {
        this->time_scale_ = scale;
        this->start_seconds = this->seconds_;
        this->start_time = this->cur_time;
    }
}

int64_t ff::timer::last_tick_raw_time() const
{
    return this->cur_time;
}

int64_t ff::timer::current_raw_time_static()
{
    int64_t cur_time;
    ::QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&cur_time));
    return cur_time;
}

int64_t ff::timer::raw_frequency_static()
{
    int64_t value;
    ::QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&value));
    return value;
}

double ff::timer::raw_frequency_double_static()
{
    static const double value = static_cast<double>(ff::timer::raw_frequency_static());
    return value;
}

int64_t ff::timer::raw_frequency() const
{
    return this->frequency;
}

double ff::timer::raw_frequency_double() const
{
    return this->frequency_double;
}

void ff::timer::store_last_tick_raw_time()
{
    this->stored_time = this->cur_time;
}

int64_t ff::timer::last_tick_stored_raw_time()
{
    int64_t diff = this->cur_time - this->stored_time;
    this->stored_time = this->cur_time;

    return diff;
}

int64_t ff::timer::current_stored_raw_time()
{
    int64_t cur_time = ff::timer::current_raw_time_static();
    int64_t diff = cur_time - this->stored_time;
    this->stored_time = cur_time;

    return diff;
}
