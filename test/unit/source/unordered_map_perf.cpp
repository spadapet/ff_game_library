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
                Assert::AreEqual(values[i], map[keys[i]]);
            }
        }

        double my_time = timer.tick();

        for (size_t i = 0; i < 8; i++)
        {
            map2.clear();

            for (size_t i = 0; i < entry_count; i++)
            {
                map2.insert_or_assign(keys[i], values[i]);
            }

            for (size_t i = 0; i < entry_count; i++)
            {
                Assert::AreEqual(values[i], map[keys[i]]);
            }
        }

        double crt_time = timer.tick();

        std::stringstream status;
        status
            << "Map with "
            << entry_count
            << " entries, 8 times: ff::unordered_map:"
            << my_time
            << ", std::unordered_map:"
            << crt_time
            << "\r\n";
        Logger::WriteMessage(status.str().c_str());
    }

    TEST_CLASS(unordered_map_perf)
    {
    public:
        TEST_METHOD(std_compare)
        {
            std_compare_perf(10);
            std_compare_perf(100);
            std_compare_perf(1000);
            //std_compare_perf(10000);
            //std_compare_perf(100000);
            //std_compare_perf(1000000);
        }
    };
}
