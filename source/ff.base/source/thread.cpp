#include "pch.h"
#include "string.h"
#include "thread.h"

void ff::set_thread_name(std::string_view name)
{
#if DEBUG
    if (!name.empty())
    {
        ::SetThreadDescription(::GetCurrentThread(), ff::string::to_wstring(name).c_str());
    }
#endif
}
