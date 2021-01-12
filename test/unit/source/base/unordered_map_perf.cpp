#include "pch.h"
#include <unordered_map>

namespace base_test
{
    static void std_compare_perf(size_t entry_count)
    {
        std::vector<std::string> keys;
        std::vector<size_t> values;
        ff::unordered_map<std::string_view, size_t> map;
        std::unordered_map<std::string_view, size_t, ff::hash<std::string_view>> map2;

        keys.reserve(entry_count);
        values.reserve(entry_count);

        for (size_t i = 0; i < entry_count; i++)
        {
            keys.push_back(std::to_string(i));
            values.push_back(i);
        }

        ff::memory::allocation_stats my_mem_stats_before = ff::memory::start_tracking_allocations();
        ff::timer timer;

        for (size_t i = 0; i < 8; i++)
        {
            map.clear();

            for (size_t i = 0; i < entry_count; i++)
            {
                map.insert_or_assign(keys[i], values[i]);
            }

            for (size_t i = 0; i < entry_count; i++)
            {
                //Assert::AreEqual(values[i], map[keys[i]]);
            }
        }

        double my_time = timer.tick();
        ff::memory::allocation_stats my_mem_stats = ff::memory::stop_tracking_allocations();
        ff::memory::allocation_stats crt_mem_stats_before = ff::memory::start_tracking_allocations();
        timer.tick();

        for (size_t i = 0; i < 8; i++)
        {
            map2.clear();

            for (size_t i = 0; i < entry_count; i++)
            {
                map2.insert_or_assign(keys[i], values[i]);
            }

            for (size_t i = 0; i < entry_count; i++)
            {
                //Assert::AreEqual(values[i], map[keys[i]]);
            }
        }

        double crt_time = timer.tick();
        ff::memory::allocation_stats crt_mem_stats = ff::memory::stop_tracking_allocations();

        // Perf results
        {
            std::ostringstream status;
            status
                << "Map with "
                << entry_count
                << " entries, 8 times: ff::unordered_map:"
                << my_time
                << ", std::unordered_map:"
                << crt_time
                << ", "
                << crt_time / my_time
                << "X"
                << "\r\n";
            Logger::WriteMessage(status.str().c_str());
        }

        // Mem results
#if _DEBUG
        {
            std::ostringstream status;
            status
                << "ff::unordered_map memory:Total:"
                << my_mem_stats.total - my_mem_stats_before.total
                << ",Max:"
                << my_mem_stats.maximum - my_mem_stats_before.maximum
                << ",Count:"
                << my_mem_stats.count - my_mem_stats_before.count
                << ". std::unordered_map memory:Total:"
                << crt_mem_stats.total - crt_mem_stats_before.total
                << ",Max:"
                << crt_mem_stats.maximum - crt_mem_stats_before.maximum
                << ",Count:"
                << crt_mem_stats.count - crt_mem_stats_before.count
                << "\r\n";
            Logger::WriteMessage(status.str().c_str());
        }
#endif
    }

    TEST_CLASS(unordered_map_perf)
    {
    public:
        TEST_METHOD(std_compare)
        {
            std_compare_perf(10);
            std_compare_perf(100);
            std_compare_perf(1000);
            std_compare_perf(10000);
            std_compare_perf(100000);
#ifndef _DEBUG
            std_compare_perf(1000000);
#endif
        }
    };
}
