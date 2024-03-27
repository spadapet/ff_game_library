#include "pch.h"
#include "app.h"
#include "filesystem.h"

std::filesystem::path ff::filesystem::app_roaming_path()
{
    std::filesystem::path path = ff::filesystem::user_roaming_path() / ff::filesystem::clean_file_name(ff::app_internal_name());
    ff::filesystem::create_directories(path);
    return path;
}

std::filesystem::path ff::filesystem::app_local_path()
{
    std::filesystem::path path = ff::filesystem::user_local_path() / ff::filesystem::clean_file_name(ff::app_internal_name());
    ff::filesystem::create_directories(path);
    return path;
}

std::filesystem::path ff::filesystem::app_temp_path()
{
    std::filesystem::path path = ff::filesystem::temp_directory_path() / "ff" / ff::filesystem::clean_file_name(ff::app_internal_name());
    ff::filesystem::create_directories(path);
    return path;
}
