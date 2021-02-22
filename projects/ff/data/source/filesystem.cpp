#include "pch.h"
#include "file.h"
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
    assert(status || !ec.value());

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

bool ff::filesystem::read_text_file(const std::filesystem::path& path, std::string& text)
{
    file_mem_mapped mm(path);
    if (!mm)
    {
        return false;
    }

    if (!mm.size())
    {
        text.clear();
        return true;
    }

    if (mm.size() >= 3 && mm.data()[0] == 0xEF && mm.data()[1] == 0xBB && mm.data()[2] == 0xBF)
    {
        // UTF-8
        text.resize(mm.size() - 3);
        std::memcpy(text.data(), mm.data() + 3, mm.size() - 3);
        return true;
    }

    if (mm.size() >= 2 && mm.data()[0] == 0xFF && mm.data()[1] == 0xFE)
    {
        // UTF-16 little endian
        text = ff::string::to_string(std::wstring_view(reinterpret_cast<const wchar_t*>(mm.data() + 2), (mm.size() - 2) / 2));
        return true;
    }

    // ignore UTF-16 big endian and UTF-32 little/big endian

    // assume ACP
    text = ff::string::from_acp(std::string_view(reinterpret_cast<const char*>(mm.data()), mm.size()));
    return true;
}

bool ff::filesystem::write_text_file(const std::filesystem::path& path, std::string_view text)
{
    std::array<uint8_t, 3> bom = { 0xEF, 0xBB, 0xBF };
    ff::file_write file(path);
    return file && file.write(bom.data(), bom.size()) && file.write(text.data(), text.size());
}
