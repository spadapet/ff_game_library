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

namespace
{
    struct thread_data_t
    {
        std::function<void()> func;
        std::string name;
    };
}

static uint32_t CALLBACK thread_callback(void* context)
{
    std::unique_ptr<::thread_data_t> data(reinterpret_cast<::thread_data_t*>(context));
    ff::set_thread_name(data->name);
    data->func();
    return 0;
}

ff::win_handle ff::create_thread(std::function<void()>&& func, std::string_view name)
{
    ::thread_data_t* data = new ::thread_data_t{ std::move(func), std::string(name) };
    return ff::win_handle(reinterpret_cast<HANDLE>(::_beginthreadex(nullptr, 0, ::thread_callback, data, 0, nullptr)));
}
