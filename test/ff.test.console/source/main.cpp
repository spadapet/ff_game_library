#include "pch.h"

void run_input_device_events();
void run_test_app();
void run_coroutine_app();
void run_game();

int main()
{
    std::cout << "Choose:" << std::endl
        << "1) Input device events" << std::endl
        << "2) Test app" << std::endl
        << "3) co_await in an app" << std::endl
        << "4) Test game loop" << std::endl;

    int choice = 0;
    std::cin >> choice;

    switch (choice)
    {
        case 1:
            ::run_input_device_events();
            break;

        case 2:
            ::run_test_app();
            break;

        case 3:
            ::run_coroutine_app();
            break;

        case 4:
            ::run_game();
            break;
    }

    return 0;
}
