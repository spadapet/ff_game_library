#include "pch.h"
#include "timer.h"

static int64_t get_raw_frequency()
{
    LARGE_INTEGER value;
    ::QueryPerformanceFrequency(&value);
    return value.QuadPart;
}

static int64_t raw_frequency = ::get_raw_frequency();
static double raw_frequency_d = static_cast<double>(::raw_frequency);
static double raw_frequency_1d = 1.0 / ::raw_frequency_d;

ff::timer::timer()
    : time_scale_(1)
{
    this->reset();
}

double ff::timer::tick(double forced_offset)
{
    this->cur_time = ff::timer::current_raw_time();
    this->clock_seconds_ = (this->cur_time - this->reset_time) * ::raw_frequency_1d;

    if (forced_offset < 0)
    {
        const double old_seconds = this->seconds_;
        this->seconds_ = this->start_seconds + (this->time_scale_ * (this->cur_time - this->start_time) * ::raw_frequency_1d);
        this->pass_seconds = std::max(this->seconds_ - old_seconds, 0.0);
    }
    else
    {
        this->pass_seconds = forced_offset;
        this->seconds_ += forced_offset;
        this->start_seconds = this->seconds_;
        this->start_time = this->cur_time;
    }

    return this->pass_seconds;
}

void ff::timer::reset()
{
    this->reset(ff::timer::current_raw_time());
}

void ff::timer::reset(int64_t cur_time)
{
    this->reset_time = cur_time;
    this->start_time = cur_time;
    this->cur_time = cur_time;
    this->start_seconds = 0;
    this->seconds_ = 0;
    this->clock_seconds_ = 0;
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

// static
int64_t ff::timer::current_raw_time()
{
    LARGE_INTEGER cur_time;
    ::QueryPerformanceCounter(&cur_time);
    return cur_time.QuadPart;
}

// static
int64_t ff::timer::raw_frequency()
{
    return ::raw_frequency;
}

// static
double ff::timer::raw_frequency_double()
{
    return ::raw_frequency_d;
}

// static
double ff::timer::seconds_between_raw(int64_t start, int64_t end)
{
    return (end - start) * ::raw_frequency_1d;
}
