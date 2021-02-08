#include "pch.h"

void run_input_device_events();

int main()
{
    std::cout << "Choose:" << std::endl
        << "1) Input device events" << std::endl;

    int choice = 0;
    std::cin >> choice;

    switch (choice)
    {
        case 1:
            run_input_device_events();
            break;
    }

    return 0;
}
