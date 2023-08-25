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
{
    this->reset();
}

double ff::timer::tick()
{
    this->now_time = ff::timer::current_raw_time();

    const double old_seconds = this->seconds_;
    this->seconds_ = (this->now_time - this->reset_time) * ::raw_frequency_1d;
    this->delta_seconds_ = std::max(this->seconds_ - old_seconds, 0.0);

    return this->delta_seconds_;
}

void ff::timer::reset()
{
    this->reset(ff::timer::current_raw_time());
}

void ff::timer::reset(int64_t now_time)
{
    this->reset_time = now_time;
    this->now_time = now_time;
    this->seconds_ = 0;
    this->delta_seconds_ = 0;
}

double ff::timer::seconds() const
{
    return this->seconds_;
}

double ff::timer::delta_seconds() const
{
    return this->delta_seconds_;
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
