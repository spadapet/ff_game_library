#include "pch.h"
#include <unordered_map>

namespace base_test
{
    static void std_compare_perf(size_t entry_count)
    {
        ff::vector<std::string> keys;
        ff::vector<size_t> values;
        ff::unordered_map<std::string_view, size_t> map;
        std::unordered_map<std::string_view, size_t, ff::hash<std::string_view>> map2;

        keys.reserve(entry_count);
        values.reserve(entry_count);

        for (size_t i = 0; i < entry_count; i++)
        {
            keys.push_back(std::to_string(i));
            values.push_back(i);
        }

        ff::memory::start_tracking_allocations();
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
        ff::memory::start_tracking_allocations();
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
            std::stringstream status;
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
        {
            std::stringstream status;
            status
                << "ff::unordered_map memory:Total:"
                << my_mem_stats.total
                << ",Max:"
                << my_mem_stats.maximum
                << ",Count:"
                << my_mem_stats.count
                << ". std::unordered_map memory:Total:"
                << crt_mem_stats.total
                << ",Max:"
                << crt_mem_stats.maximum
                << ",Count:"
                << crt_mem_stats.count
                << "\r\n";
            Logger::WriteMessage(status.str().c_str());
        }
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
