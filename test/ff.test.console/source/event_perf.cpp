#include "pch.h"

static void run_event_perf_internal(bool caching, size_t iterations, size_t task_count)
{
    std::vector<ff::win_handle> cache;
    cache.reserve(task_count);

    for (size_t i = 0; i < iterations; i++)
    {
        ff::stack_vector<ff::win_handle, 32> handles;
        ff::stack_vector<ff::win_handle, 32> dupe_handles;
        ff::stack_vector<HANDLE, 32> raw_handles;

        for (size_t h = 0; h < task_count; h++)
        {
            if (!cache.empty())
            {
                handles.push_back(std::move(cache.back()));
                cache.pop_back();
            }
            else
            {
                handles.push_back(ff::win_handle::create_event());
            }

            raw_handles.push_back(handles.back());

            dupe_handles.push_back(ff::win_handle(handles.back()));
            raw_handles.push_back(dupe_handles.back());
        }

        for (auto& h : handles)
        {
            ff::thread_pool::add_task([&h]
                {
                    ::SetEvent(h);
                });
        }

        ff::wait_for_all_handles(raw_handles.data(), raw_handles.size());

        if (caching)
        {
            for (auto& h : handles)
            {
                ::ResetEvent(h);
                cache.push_back(std::move(h));
            }
        }
    }
}

void run_event_perf()
{
    ff::timer timer;

    ::run_event_perf_internal(true, 100000, 8);
    double cache_time = timer.tick();
    ::run_event_perf_internal(false, 100000, 8);
    double no_cache_time = timer.tick();

    std::cout
        << "Cache time: " << cache_time * 1000 << "ms" << std::endl
        << "No cache time: " << no_cache_time * 1000 << "ms" << std::endl;
}
