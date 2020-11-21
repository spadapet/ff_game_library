#include "pch.h"

static void std_compare_perf(size_t entry_count)
{
    ff::vector<std::string> keys;
    ff::vector<size_t> values;
    ff::unordered_map<std::string_view, size_t> map;
    std::unordered_map<std::string_view, size_t, ff::hash<std::string_view>> map2;

    keys.reserve(entry_count);
    values.reserve(entry_count);
    map.reserve(entry_count);
    map2.reserve(entry_count);

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
            map.emplace(keys[i], values[i]);
        }

        for (size_t i = 0; i < entry_count; i++)
        {
            if (values[i] != map[keys[i]])
            {
                std::cerr << "Unexpected value" << std::endl;
            }
        }
    }

    double my_time = timer.tick();

    for (size_t i = 0; i < 8; i++)
    {
        map2.clear();
        
        for (size_t i = 0; i < entry_count; i++)
        {
            map2.emplace(keys[i], values[i]);
        }

        for (size_t i = 0; i < entry_count; i++)
        {
            if (values[i] != map2[keys[i]])
            {
                std::cerr << "Unexpected value" << std::endl;
            }
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
        << ", "
        << crt_time / my_time
        << "X";

    std::cout << status.str() << std::endl;
}

int main()
{
    ::std_compare_perf(10);
    ::std_compare_perf(100);
    ::std_compare_perf(1000);
    ::std_compare_perf(10000);
    ::std_compare_perf(100000);
    ::std_compare_perf(1000000);

    return 0;
}
