#include "pch.h"
#include "perf_timer.h"

ff::perf_timer::perf_timer(std::string_view name)
    : name(name)
    , start_time(__rdtsc())
{}

ff::perf_timer::~perf_timer()
{

}
