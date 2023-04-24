#include "pch.h"
#include "assert.h"
#include "constants.h"
#include "filesystem.h"
#include "string.h"

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
    ff::filesystem::create_directories(path);
    return path;
}

#endif

bool ff::filesystem::exists(const std::filesystem::path& path)
{
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

bool ff::filesystem::equivalent(const std::filesystem::path& lhs, const std::filesystem::path& rhs)
{
    std::error_code ec;
    return std::filesystem::equivalent(lhs, rhs, ec);
}

std::filesystem::file_time_type ff::filesystem::last_write_time(const std::filesystem::path& path)
{
    std::error_code ec;
    return std::filesystem::last_write_time(path, ec);
}

bool ff::filesystem::create_directories(const std::filesystem::path& path)
{
    std::error_code ec;
    return std::filesystem::create_directories(path, ec) || !ec.value();
}

size_t ff::filesystem::file_size(const std::filesystem::path& path)
{
    std::error_code ec;
    uintmax_t size = std::filesystem::file_size(path, ec);
    return (size != ff::constants::invalid_uintmax) ? static_cast<size_t>(size) : ff::constants::invalid_size;
}

bool ff::filesystem::remove(const std::filesystem::path& path)
{
    if (!ff::filesystem::exists(path))
    {
        return true;
    }

    std::error_code ec;
    return std::filesystem::remove(path, ec) || !ec.value();
}

bool ff::filesystem::remove_all(const std::filesystem::path& path)
{
    if (!ff::filesystem::exists(path))
    {
        return true;
    }

    std::error_code ec;
    return std::filesystem::remove_all(path, ec) != ff::constants::invalid_uintmax || !ec.value();
}

std::filesystem::path ff::filesystem::weakly_canonical(const std::filesystem::path& path)
{
    std::error_code ec;
    return std::filesystem::weakly_canonical(path, ec);
}

std::string ff::filesystem::to_string(const std::filesystem::path& path)
{
    return ff::string::to_string(path.native());
}

std::filesystem::path ff::filesystem::to_path(std::string_view path)
{
    return std::filesystem::path(ff::string::to_wstring(path));
}

std::filesystem::path ff::filesystem::executable_path()
{
    wchar_t* wstr{};
    ::_get_wpgmptr(&wstr);
    return ff::filesystem::to_path(ff::string::to_string(wstr));
}

std::filesystem::path ff::filesystem::temp_directory_path()
{
#if UWP_APP
    return ff::string::to_string(winrt::Windows::Storage::ApplicationData::Current().TemporaryFolder().Path());
#else
    return ::append_ff_game_engine_directory(std::filesystem::temp_directory_path());
#endif
}

std::filesystem::path ff::filesystem::user_local_path()
{
#if UWP_APP
    return ff::string::to_string(winrt::Windows::Storage::ApplicationData::Current().LocalFolder().Path());
#else
    return ::get_known_directory(FOLDERID_LocalAppData, true);
#endif
}

std::filesystem::path ff::filesystem::user_roaming_path()
{
#if UWP_APP
    return ff::string::to_string(winrt::Windows::Storage::ApplicationData::Current().RoamingFolder().Path());
#else
    return ::get_known_directory(FOLDERID_RoamingAppData, true);
#endif
}

std::filesystem::path ff::filesystem::to_lower(const std::filesystem::path& path)
{
    std::wstring wstr = path.native();

    wstr.push_back(0);
    ::_wcslwr_s(wstr.data(), static_cast<DWORD>(wstr.size()));
    wstr.pop_back();

    return std::filesystem::path(wstr);
}

std::filesystem::path ff::filesystem::clean_file_name(const std::filesystem::path& path)
{
    std::string new_string = ff::string::to_string(path.native());
    const std::string_view replace_char = R"(<>:"/\|?*)";
    const std::string_view replace_with = R"(()-'--_--)";

    for (char& ch : new_string)
    {
        if ((ch >= 0 && ch < ' ') || ch == 0x7F)
        {
            ch = ' ';
        }
        else
        {
            size_t i = replace_char.find(ch);
            if (i != std::string_view::npos)
            {
                ch = replace_with[i];
            }
        }
    }

    std::filesystem::path new_path;
    {
        std::ostringstream new_stream;
        for (std::string_view token : ff::string::split(new_string, " "))
        {
            if (new_stream.tellp() > 0)
            {
                new_stream << " ";
            }

            new_stream << token;
        }

        new_path = ff::string::to_wstring(new_stream.str());
    }

    std::filesystem::path extension = new_path.extension();
    new_path.replace_extension();

    if (!::_wcsicmp(new_path.c_str(), L"AUX") ||
        !::_wcsicmp(new_path.c_str(), L"CLOCK$") ||
        !::_wcsicmp(new_path.c_str(), L"COM1") ||
        !::_wcsicmp(new_path.c_str(), L"COM2") ||
        !::_wcsicmp(new_path.c_str(), L"COM3") ||
        !::_wcsicmp(new_path.c_str(), L"COM4") ||
        !::_wcsicmp(new_path.c_str(), L"COM5") ||
        !::_wcsicmp(new_path.c_str(), L"COM6") ||
        !::_wcsicmp(new_path.c_str(), L"COM7") ||
        !::_wcsicmp(new_path.c_str(), L"COM8") ||
        !::_wcsicmp(new_path.c_str(), L"COM9") ||
        !::_wcsicmp(new_path.c_str(), L"CON") ||
        !::_wcsicmp(new_path.c_str(), L"LPT1") ||
        !::_wcsicmp(new_path.c_str(), L"LPT2") ||
        !::_wcsicmp(new_path.c_str(), L"LPT3") ||
        !::_wcsicmp(new_path.c_str(), L"LPT4") ||
        !::_wcsicmp(new_path.c_str(), L"LPT5") ||
        !::_wcsicmp(new_path.c_str(), L"LPT6") ||
        !::_wcsicmp(new_path.c_str(), L"LPT7") ||
        !::_wcsicmp(new_path.c_str(), L"LPT8") ||
        !::_wcsicmp(new_path.c_str(), L"LPT9") ||
        !::_wcsicmp(new_path.c_str(), L"NUL") ||
        !::_wcsicmp(new_path.c_str(), L"PRN"))
    {
        new_path = new_path.native() + L"_file";
    }

    return new_path.replace_extension(extension);
}
