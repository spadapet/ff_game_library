#include "pch.h"
#include "file.h"

ff::data::file_base::file_base()
    : file_handle(INVALID_HANDLE_VALUE)
{
}

ff::data::file_base::file_base(const std::filesystem::path& path)
    : file_path(path)
    , file_handle(INVALID_HANDLE_VALUE)
{
}

ff::data::file_base::file_base(file_base&& other) noexcept
    : file_path(std::move(other.file_path))
    , file_handle(other.file_handle)
{
    other.file_handle = INVALID_HANDLE_VALUE;
}

ff::data::file_base::~file_base()
{
    this->handle(INVALID_HANDLE_VALUE);
}

size_t ff::data::file_base::size() const
{
    assert(this->file_handle != INVALID_HANDLE_VALUE);
    
    FILE_STANDARD_INFO info;
    if (!::GetFileInformationByHandleEx(this->file_handle, FileStandardInfo, &info, sizeof(info)))
    {
        assert(false);
        return 0;
    }

    return static_cast<size_t>(info.EndOfFile.QuadPart);
}

size_t ff::data::file_base::pos() const
{
    assert(this->file_handle != INVALID_HANDLE_VALUE);
    
    LARGE_INTEGER cur{}, move{};
    if (!::SetFilePointerEx(this->file_handle, move, &cur, FILE_CURRENT))
    {
        assert(false);
        return 0;
    }

    return static_cast<size_t>(cur.QuadPart);
}

size_t ff::data::file_base::pos(size_t new_pos)
{
    assert(this->file_handle != INVALID_HANDLE_VALUE);

    LARGE_INTEGER cur{}, move{};
    move.QuadPart = new_pos;
    if (!::SetFilePointerEx(this->file_handle, move, &cur, FILE_BEGIN))
    {
        assert(false);
        return 0;
    }

    return static_cast<size_t>(cur.QuadPart);
}

HANDLE ff::data::file_base::handle() const
{
    return this->file_handle;
}

const std::filesystem::path& ff::data::file_base::path() const
{
    return this->file_path;
}

ff::data::file_base::operator bool() const
{
    return this->file_handle != INVALID_HANDLE_VALUE;
}

bool ff::data::file_base::operator!() const
{
    return this->file_handle == INVALID_HANDLE_VALUE;
}

ff::data::file_base& ff::data::file_base::operator=(file_base&& other) noexcept
{
    if (this != &other)
    {
        this->handle(INVALID_HANDLE_VALUE);
        this->file_path.clear();

        std::swap(this->file_handle, other.file_handle);
        std::swap(this->file_path, other.file_path);
    }

    return *this;
}

ff::data::file_base& ff::data::file_base::operator=(const file_base& other)
{
    if (this != &other)
    {
        this->handle(INVALID_HANDLE_VALUE);
        this->file_path = other.file_path;

        if (other)
        {
            HANDLE file_handle = INVALID_HANDLE_VALUE;
            if (::DuplicateHandle(::GetCurrentProcess(), other.handle(), ::GetCurrentProcess(), &file_handle, 0, FALSE, DUPLICATE_SAME_ACCESS))
            {
                this->handle(file_handle);
            }

            assert(file_handle != INVALID_HANDLE_VALUE);
        }
    }

    return *this;
}

void ff::data::file_base::handle(HANDLE file_handle)
{
    if (this->file_handle != INVALID_HANDLE_VALUE)
    {
        ::CloseHandle(this->file_handle);
    }

    this->file_handle = file_handle;
}

ff::data::file_read::file_read(const std::filesystem::path& path)
    : file_base(path)
{
    std::wstring wpath = ff::string::to_wstring(path.string());
    HANDLE file_handle = ::CreateFile2(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, nullptr);
    assert(file_handle != INVALID_HANDLE_VALUE);
    this->handle(file_handle);
}

ff::data::file_read::file_read(file_read&& other) noexcept
    : file_base(std::move(other))
{
}

ff::data::file_read::file_read(const file_read& other)
{
    *this = other;
}

ff::data::file_read& ff::data::file_read::operator=(file_read&& other) noexcept
{
    file_base::operator=(std::move(other));
    return *this;
}

ff::data::file_read& ff::data::file_read::operator=(const file_read& other)
{
    file_base::operator=(other);
    return *this;
}

size_t ff::data::file_read::read(void* data, size_t size)
{
    DWORD read = 0;
    if (size && ::ReadFile(this->handle(), data, static_cast<DWORD>(size), &read, nullptr))
    {
        return static_cast<size_t>(read);
    }

    return 0;
}

ff::data::file_write::file_write(const std::filesystem::path& path, bool append)
    : file_base(path)
{
    std::wstring wpath = ff::string::to_wstring(path.string());
    HANDLE file_handle = ::CreateFile2(wpath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, append ? OPEN_ALWAYS : CREATE_ALWAYS, nullptr);
    assert(file_handle != INVALID_HANDLE_VALUE);
    this->handle(file_handle);

    if (append && file_handle != INVALID_HANDLE_VALUE)
    {
        this->pos(this->size());
    }
}

ff::data::file_write::file_write(file_write&& other) noexcept
    : file_base(std::move(other))
{
}

ff::data::file_write& ff::data::file_write::operator=(file_write&& other) noexcept
{
    file_base::operator=(std::move(other));
    return *this;
}

size_t ff::data::file_write::write(const void* data, size_t size)
{
    DWORD written = 0;
    if (size && ::WriteFile(this->handle(), data, static_cast<DWORD>(size), &written, nullptr))
    {
        return static_cast<size_t>(written);
    }

    return 0;
}

ff::data::file_mem_mapped::file_mem_mapped(const std::filesystem::path& path)
    : file_mem_mapped(file_read(path))
{
}

ff::data::file_mem_mapped::file_mem_mapped(file_read&& file) noexcept
    : mapping_file(std::move(file))
    , mapping_handle(nullptr)
    , mapping_size(0)
    , mapping_data(nullptr)
{
    this->open();
}

ff::data::file_mem_mapped::file_mem_mapped(file_mem_mapped&& other) noexcept
    : mapping_file(std::move(other.mapping_file))
    , mapping_handle(other.mapping_handle)
    , mapping_size(other.mapping_size)
    , mapping_data(other.mapping_data)
{
    other.mapping_handle = nullptr;
    other.mapping_size = 0;
    other.mapping_data = nullptr;
}

ff::data::file_mem_mapped::file_mem_mapped(const file_read& file)
    : file_mem_mapped(file_read(file))
{
}

ff::data::file_mem_mapped::file_mem_mapped(const file_mem_mapped& other)
    : file_mem_mapped(other.mapping_file)
{
}

ff::data::file_mem_mapped::~file_mem_mapped()
{
    this->close();
}

ff::data::file_mem_mapped::operator bool() const
{
    return this->mapping_handle && this->mapping_data;
}

bool ff::data::file_mem_mapped::operator!() const
{
    return !this->mapping_handle || !this->mapping_data;
}

ff::data::file_mem_mapped& ff::data::file_mem_mapped::operator=(file_mem_mapped&& other) noexcept
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

size_t ff::data::file_mem_mapped::size() const
{
    return this->mapping_size;
}

const uint8_t* ff::data::file_mem_mapped::data() const
{
    return this->mapping_data;
}

const std::filesystem::path& ff::data::file_mem_mapped::path() const
{
    return this->mapping_file.path();
}

const ff::data::file_read& ff::data::file_mem_mapped::file() const
{
    return this->mapping_file;
}

void ff::data::file_mem_mapped::open()
{
    this->close();

    if (this->mapping_file && this->mapping_file.size())
    {
#if METRO_APP
        this->mapping_handle = ::CreateFileMappingFromApp(this->file.handle(), nullptr, PAGE_READONLY, 0, nullptr);
#else
        this->mapping_handle = ::CreateFileMapping(this->mapping_file.handle(), nullptr, PAGE_READONLY, 0, 0, nullptr);
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

void ff::data::file_mem_mapped::close()
{
    if (this->mapping_data)
    {
        ::UnmapViewOfFile(this->mapping_data);
        this->mapping_data = nullptr;
        this->mapping_size = 0;
    }

    if (this->mapping_handle)
    {
        ::CloseHandle(this->mapping_handle);
        this->mapping_handle = nullptr;
    }
}
