#include "pch.h"

void run_test_input_devices();
void run_test_app();
void run_test_coroutine_app();
void run_test_game_wrapper();
void run_test_sprite_perf();

int main()
{
    std::cout << "Choose:\n"
        << "1) Test input devices\n"
        << "2) Test blank app\n"
        << "3) Test co_await in blank app\n"
        << "4) Test ff::game::run with moving colors\n"
        << "5) Test sprite perf\n";

    int choice{};
    std::cin >> choice;

    switch (choice)
    {
        case 1:
            ::run_test_input_devices();
            break;

        case 2:
            ::run_test_app();
            break;

        case 3:
            ::run_test_coroutine_app();
            break;

        case 4:
            ::run_test_game_wrapper();
            break;

        case 5:
            ::run_test_sprite_perf();
            break;
    }

    return 0;
}
