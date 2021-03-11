#include "pch.h"
#include "app.h"
#include "filesystem.h"

std::filesystem::path ff::filesystem::app_roaming_path()
{
    std::error_code ec;
#if UWP_APP
    std::filesystem::path path = ff::filesystem::user_roaming_path();
#else
    std::filesystem::path path = ff::filesystem::user_roaming_path() / ff::filesystem::clean_file_name(ff::app_name());
#endif
    std::filesystem::create_directories(path, ec);
    return path;
}

std::filesystem::path ff::filesystem::app_local_path()
{
    std::error_code ec;
#if UWP_APP
    std::filesystem::path path = ff::filesystem::user_local_path();
#else
    std::filesystem::path path = ff::filesystem::user_local_path() / ff::filesystem::clean_file_name(ff::app_name());
#endif
    std::filesystem::create_directories(path, ec);
    return path;
}

std::filesystem::path ff::filesystem::app_temp_path()
{
    std::error_code ec;
#if UWP_APP
    std::filesystem::path path = ff::filesystem::temp_directory_path();
#else
    std::filesystem::path path = ff::filesystem::temp_directory_path() / "ff" / ff::filesystem::clean_file_name(ff::app_name());
#endif
    std::filesystem::create_directories(path, ec);
    return path;
}
