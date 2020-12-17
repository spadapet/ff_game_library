#include "pch.h"
#include "filesystem.h"

#if !UWP_APP

static std::filesystem::path get_known_directory(REFGUID id, bool create)
{
    wchar_t* wpath = nullptr;
    if (SUCCEEDED(::SHGetKnownFolderPath(id, create ? KF_FLAG_CREATE : 0, nullptr, &wpath)))
    {
        std::filesystem::path path = ff::string::to_string(wpath);
        ::CoTaskMemFree(wpath);
        return path;
    }

    // Fallback just in case...
    assert(false);
    return ff::filesystem::temp_directory_path();
}

static std::filesystem::path append_ff_game_engine_directory(std::filesystem::path path)
{
    path /= "ff_game_engine";

    std::error_code ec;
    bool status = std::filesystem::create_directories(path, ec);
    assert(status);

    return path;
}

#endif

std::filesystem::path ff::filesystem::temp_directory_path()
{
#if UWP_APP
    return ff::string::to_string(Windows::Storage::ApplicationData::Current->TemporaryFolder->Path);
#else
    return ::append_ff_game_engine_directory(std::filesystem::temp_directory_path());
#endif
}

std::filesystem::path ff::filesystem::user_directory_path()
{
#if UWP_APP
    return ff::string::to_string(Windows::Storage::ApplicationData::Current->LocalFolder->Path);
#else
    return ::append_ff_game_engine_directory(::get_known_directory(FOLDERID_LocalAppData, true));
#endif
}
