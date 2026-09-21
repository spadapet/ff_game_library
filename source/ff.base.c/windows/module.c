#include "pch.h"
#include "windows/module.h"

extern IMAGE_DOS_HEADER __ImageBase;

HINSTANCE ff_module_instance(void)
{
    HINSTANCE instance = (HINSTANCE)&__ImageBase;
    return instance ? instance : GetModuleHandle(NULL);
}
