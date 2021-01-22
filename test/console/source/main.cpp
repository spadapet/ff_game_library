#include "pch.h"

void run_hash_table_perf();
void run_input_device_events();

int main()
{
    std::cout << "Choose:" << std::endl
        << "1) Hash table perf" << std::endl
        << "2) Input device events" << std::endl;

    int choice = 0;
    std::cin >> choice;

    switch (choice)
    {
        case 1:
            run_hash_table_perf();
            break;

        case 2:
            run_input_device_events();
            break;
    }

    return 0;
}
