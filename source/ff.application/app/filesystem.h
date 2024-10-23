#pragma once

namespace ff::filesystem
{
    std::filesystem::path app_roaming_path();
    std::filesystem::path app_local_path();
    std::filesystem::path app_temp_path();
}
