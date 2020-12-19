#include "pch.h"
#include "file.h"

ff::file_base::file_base()
{}

ff::file_base::file_base(const std::filesystem::path& path)
    : file_path(path)
{}

ff::file_base::file_base(file_base&& other) noexcept
    : file_path(std::move(other.file_path))
    , file_handle(std::move(other.file_handle))
{
}

ff::file_base::~file_base()
{
}

size_t ff::file_base::size() const
{
    assert(this->file_handle);

    FILE_STANDARD_INFO info;
    if (!::GetFileInformationByHandleEx(this->file_handle, FileStandardInfo, &info, sizeof(info)))
    {
        assert(false);
        return 0;
    }

    return static_cast<size_t>(info.EndOfFile.QuadPart);
}

size_t ff::file_base::pos() const
{
    assert(this->file_handle);

    LARGE_INTEGER cur{}, move{};
    if (!::SetFilePointerEx(this->file_handle, move, &cur, FILE_CURRENT))
    {
        assert(false);
        return 0;
    }

    return static_cast<size_t>(cur.QuadPart);
}

size_t ff::file_base::pos(size_t new_pos)
{
    assert(this->file_handle);

    LARGE_INTEGER cur{}, move{};
    move.QuadPart = new_pos;
    if (!::SetFilePointerEx(this->file_handle, move, &cur, FILE_BEGIN))
    {
        assert(false);
        return 0;
    }

    return static_cast<size_t>(cur.QuadPart);
}

const ff::win_handle& ff::file_base::handle() const
{
    return this->file_handle;
}

const std::filesystem::path& ff::file_base::path() const
{
    return this->file_path;
}

ff::file_base::operator bool() const
{
    return this->file_handle;
}

bool ff::file_base::operator!() const
{
    return !this->file_handle;
}

ff::file_base& ff::file_base::operator=(file_base&& other) noexcept
{
    if (this != &other)
    {
        this->file_path.clear();

        std::swap(this->file_handle, other.file_handle);
        std::swap(this->file_path, other.file_path);
    }

    return *this;
}

ff::file_base& ff::file_base::operator=(const file_base& other)
{
    if (this != &other)
    {
        this->file_path = other.file_path;
        this->file_handle = other.file_handle.duplicate();
    }

    return *this;
}

void ff::file_base::handle(win_handle&& file_handle)
{
    this->file_handle = std::move(file_handle);
}

ff::file_read::file_read(const std::filesystem::path& path)
    : file_base(path)
{
    std::wstring wpath = ff::string::to_wstring(path.string());
    win_handle file_handle(::CreateFile2(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, nullptr));
    assert(file_handle);
    this->handle(std::move(file_handle));
}

ff::file_read::file_read(file_read&& other) noexcept
    : file_base(std::move(other))
{}

ff::file_read::file_read(const file_read& other)
{
    *this = other;
}

ff::file_read& ff::file_read::operator=(file_read&& other) noexcept
{
    file_base::operator=(std::move(other));
    return *this;
}

ff::file_read& ff::file_read::operator=(const file_read& other)
{
    file_base::operator=(other);
    return *this;
}

size_t ff::file_read::read(void* data, size_t size)
{
    DWORD read = 0;
    if (size && ::ReadFile(this->handle(), data, static_cast<DWORD>(size), &read, nullptr))
    {
        return static_cast<size_t>(read);
    }

    return 0;
}

ff::file_write::file_write(const std::filesystem::path& path, bool append)
    : file_base(path)
{
    std::error_code ec;
    std::filesystem::path parent_path = path.parent_path();
    if (!parent_path.empty() && !std::filesystem::exists(parent_path, ec))
    {
        // Ignore failure here, let CreateFile fail
        std::filesystem::create_directories(parent_path, ec);
    }

    std::wstring wpath = ff::string::to_wstring(path.string());
    win_handle file_handle(::CreateFile2(wpath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, append ? OPEN_ALWAYS : CREATE_ALWAYS, nullptr));
    assert(file_handle);
    this->handle(std::move(file_handle));

    if (append && this->handle())
    {
        this->pos(this->size());
    }
}

ff::file_write::file_write(file_write&& other) noexcept
    : file_base(std::move(other))
{}

ff::file_write& ff::file_write::operator=(file_write&& other) noexcept
{
    file_base::operator=(std::move(other));
    return *this;
}

size_t ff::file_write::write(const void* data, size_t size)
{
    DWORD written = 0;
    if (size && ::WriteFile(this->handle(), data, static_cast<DWORD>(size), &written, nullptr))
    {
        return static_cast<size_t>(written);
    }

    return 0;
}

ff::file_mem_mapped::file_mem_mapped(const std::filesystem::path& path)
    : file_mem_mapped(file_read(path))
{}

ff::file_mem_mapped::file_mem_mapped(file_read&& file) noexcept
    : mapping_file(std::move(file))
    , mapping_size(0)
    , mapping_data(nullptr)
{
    this->open();
}

ff::file_mem_mapped::file_mem_mapped(file_mem_mapped&& other) noexcept
    : mapping_file(std::move(other.mapping_file))
    , mapping_handle(std::move(other.mapping_handle))
    , mapping_size(other.mapping_size)
    , mapping_data(other.mapping_data)
{
    other.mapping_size = 0;
    other.mapping_data = nullptr;
}

ff::file_mem_mapped::file_mem_mapped(const file_read& file)
    : file_mem_mapped(file_read(file))
{}

ff::file_mem_mapped::file_mem_mapped(const file_mem_mapped& other)
    : file_mem_mapped(other.mapping_file)
{}

ff::file_mem_mapped::~file_mem_mapped()
{
    this->close();
}

ff::file_mem_mapped::operator bool() const
{
    return this->mapping_handle && this->mapping_data;
}

bool ff::file_mem_mapped::operator!() const
{
    return !this->mapping_handle || !this->mapping_data;
}

ff::file_mem_mapped& ff::file_mem_mapped::operator=(file_mem_mapped&& other) noexcept
{
    if (this != &other)
    {
        this->close();

        std::swap(this->mapping_file, other.mapping_file);
        std::swap(this->mapping_handle, other.mapping_handle);
        std::swap(this->mapping_size, other.mapping_size);
        std::swap(this->mapping_data, other.mapping_data);
    }

    return *this;
}

size_t ff::file_mem_mapped::size() const
{
    return this->mapping_size;
}

const uint8_t* ff::file_mem_mapped::data() const
{
    return this->mapping_data;
}

const std::filesystem::path& ff::file_mem_mapped::path() const
{
    return this->mapping_file.path();
}

const ff::file_read& ff::file_mem_mapped::file() const
{
    return this->mapping_file;
}

void ff::file_mem_mapped::open()
{
    this->close();

    if (this->mapping_file && this->mapping_file.size())
    {
#if METRO_APP
        this->mapping_handle = win_handle(::CreateFileMappingFromApp(this->file.handle(), nullptr, PAGE_READONLY, 0, nullptr));
#else
        this->mapping_handle = win_handle(::CreateFileMapping(this->mapping_file.handle(), nullptr, PAGE_READONLY, 0, 0, nullptr));
#endif
        assert(this->mapping_handle);

        if (this->mapping_handle)
        {
            this->mapping_size = this->mapping_file.size();
#if METRO_APP
            this->mapping_data = reinterpret_cast<const uint8_t*>(::MapViewOfFileFromApp(this->mapping_handle, FILE_MAP_READ, 0, _size));
#else
            this->mapping_data = reinterpret_cast<const uint8_t*>(::MapViewOfFile(this->mapping_handle, FILE_MAP_READ, 0, 0, this->mapping_size));
#endif
            assert(this->mapping_data);
        }
    }
}

void ff::file_mem_mapped::close()
{
    if (this->mapping_data)
    {
        ::UnmapViewOfFile(this->mapping_data);
        this->mapping_data = nullptr;
        this->mapping_size = 0;
    }

    this->mapping_handle.close();
}
