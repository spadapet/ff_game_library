#pragma once

namespace ff
{
    class perf_timer
    {
    public:
        perf_timer(std::string_view name);
        ~perf_timer();

    private:
        const std::string_view name;
        int64_t start_time;
    };
}
