#include "pch.h"

void run_test_app();

int main()
{
    std::cout << "Choose:" << std::endl
        << "1) DirectX 12 graphics" << std::endl;

    int choice = 0;
    std::cin >> choice;

    switch (choice)
    {
        case 1:
            ::run_test_app();
            break;
    }

    return 0;
}
