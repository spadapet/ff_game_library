#include "pch.h"
#include "perf_timer.h"

ff::perf_timer::perf_timer(const ff::perf_counter& counter)
    : counter(counter)
    , start(__rdtsc())
{}

ff::perf_timer::~perf_timer()
{

}
