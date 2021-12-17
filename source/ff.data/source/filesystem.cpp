#include "pch.h"
#include "data.h"
#include "file.h"
#include "filesystem.h"

std::shared_ptr<ff::data_base> ff::filesystem::read_binary_file(const std::filesystem::path& path)
{
    ff::file_read file(path);
    if (file)
    {
        std::vector<uint8_t> bytes;
        if (file.size())
        {
            bytes.resize(file.size());
            if (file.read(bytes.data(), bytes.size()) != file.size())
            {
                assert(false);
                return nullptr;
            }
        }

        return std::make_shared<ff::data_vector>(std::move(bytes));
    }

    return nullptr;
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

bool ff::filesystem::write_binary_file(const std::filesystem::path& path, const void* data, size_t size)
{
    ff::file_write file(path);
    return file && file.write(data, size);
}

bool ff::filesystem::write_text_file(const std::filesystem::path& path, std::string_view text)
{
    std::array<uint8_t, 3> bom = { 0xEF, 0xBB, 0xBF };
    ff::file_write file(path);
    return file && file.write(bom.data(), bom.size()) && file.write(text.data(), text.size());
}
