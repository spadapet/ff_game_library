#include "pch.h"

void run_test_app();
void run_test_coroutine_app();
void run_test_sprite_perf();
void run_test_game_wrapper();

int main()
{
    std::cout << "Choose:\n"
        << "1) Test input in app\n"
        << "2) Test co_await in app\n"
        << "3) Test sprite perf in app\n"
        << "4) Test moving colors in game\n";

    int choice{};
    std::cin >> choice;

    switch (choice)
    {
        case 1:
            ::run_test_app();
            break;

        case 2:
            ::run_test_coroutine_app();
            break;

        case 3:
            ::run_test_sprite_perf();
            break;

        case 4:
            ::run_test_game_wrapper();
            break;
    }

    return 0;
}
