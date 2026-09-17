#include "pch.h"

int main()
{
    printf("Hello World!\n");

    ff_dx12_init_params params = ff_dx12_init_params_default();
    if (ff_dx12_init(&params))
    {
        ff_dx12_device_valid();
        ff_dx12_destroy();
    }

    return 0;
}
