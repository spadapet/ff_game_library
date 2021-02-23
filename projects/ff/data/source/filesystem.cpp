#include "pch.h"
#include "file.h"
#include "filesystem.h"

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
