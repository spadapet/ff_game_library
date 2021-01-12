#include "pch.h"

static void std_compare_perf(size_t entry_count)
{
    std::vector<std::string> keys;
    std::vector<size_t> values;
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
        << "X";

    std::cout << status.str() << std::endl;
}

static void run_hash_table_perf()
{
    ::std_compare_perf(10);
    ::std_compare_perf(100);
    ::std_compare_perf(1000);
    ::std_compare_perf(10000);
    ::std_compare_perf(100000);
    ::std_compare_perf(1000000);
}

static void run_unicode_file_name()
{
    std::filesystem::path temp_path = ff::filesystem::temp_directory_path() / "👌.txt";
    std::ofstream file(temp_path);
    file.write("Hello world!\r\n", 14);
    file.close();

    std::error_code ec;
    std::filesystem::remove(temp_path, ec);
}

int main()
{
    ff::init_resource init;

    std::cout << "Choose:" << std::endl
        << "1) Hash table perf" << std::endl
        << "2) Unicode file names" << std::endl;

    int choice = 0;
    std::cin >> choice;

    switch (choice)
    {
        case 1:
            run_hash_table_perf();
            break;

        case 2:
            run_unicode_file_name();
            break;
    }

    return 0;
}
