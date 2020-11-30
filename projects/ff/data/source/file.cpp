#include "pch.h"
#include "file.h"

size_t ff::data::internal::file_base::size() const
{
    assert(this->handle_data != INVALID_HANDLE_VALUE);
    
    FILE_STANDARD_INFO info;
    if (!::GetFileInformationByHandleEx(this->handle_data, FileStandardInfo, &info, sizeof(info)))
    {
        assert(false);
        return 0;
    }

    return static_cast<size_t>(info.EndOfFile.QuadPart);
}

size_t ff::data::internal::file_base::pos() const
{
    assert(this->handle_data != INVALID_HANDLE_VALUE);
    
    LARGE_INTEGER cur{}, move{};
    if (!::SetFilePointerEx(this->handle_data, move, &cur, FILE_CURRENT))
    {
        assert(false);
        return 0;
    }

    return static_cast<size_t>(cur.QuadPart);
}

size_t ff::data::internal::file_base::pos(size_t new_pos)
{
    assert(this->handle_data != INVALID_HANDLE_VALUE);

    LARGE_INTEGER cur{}, move{};
    move.QuadPart = new_pos;
    if (!::SetFilePointerEx(this->handle_data, move, &cur, FILE_BEGIN))
    {
        assert(false);
        return 0;
    }

    return static_cast<size_t>(cur.QuadPart);
}

ff::data::internal::file_base::file_base()
    : handle_data(INVALID_HANDLE_VALUE)
{
}

ff::data::internal::file_base::file_base(file_base&& other) noexcept
    : file_base()
{
    *this = std::move(other);
}

ff::data::internal::file_base::~file_base()
{
    this->handle(INVALID_HANDLE_VALUE);
}

ff::data::internal::file_base::operator bool() const
{
    return this->handle_data != INVALID_HANDLE_VALUE;
}

bool ff::data::internal::file_base::operator!() const
{
    return this->handle_data == INVALID_HANDLE_VALUE;
}

ff::data::internal::file_base& ff::data::internal::file_base::operator=(file_base&& other) noexcept
{
    if (this != &other)
    {
        this->handle(INVALID_HANDLE_VALUE);
        this->swap(other);
    }

    return *this;
}

void ff::data::internal::file_base::swap(file_base& other)
{
    std::swap(this->handle_data, other.handle_data);
}

HANDLE ff::data::internal::file_base::handle() const
{
    return this->handle_data;
}

void ff::data::internal::file_base::handle(HANDLE handle_data)
{
    if (this->handle_data != INVALID_HANDLE_VALUE)
    {
        ::CloseHandle(this->handle_data);
    }

    this->handle_data = handle_data;
}

ff::data::file_read::file_read(std::string_view path)
    : file_read(std::filesystem::path(path))
{
}

ff::data::file_read::file_read(const std::filesystem::path& path)
    : file_path(path)
{
    HANDLE handle_data = ::CreateFile2(ff::string::to_wstring(path.string()).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, nullptr);
    assert(handle_data != INVALID_HANDLE_VALUE);
    this->handle(handle_data);
}

ff::data::file_read::file_read(file_read&& other)
{
}

ff::data::file_read::file_read(file_read&& other) noexcept
{
    *this = std::move(other);
}

ff::data::file_read::file_read(const file_read& other)
{
    *this = other;
}

ff::data::file_read& ff::data::file_read::operator=(file_read&& other) noexcept
{
    if (this != &other)
    {
        this->handle(INVALID_HANDLE_VALUE);
        this->swap(other);
    }

    return *this;
}

ff::data::file_read& ff::data::file_read::operator=(const file_read& other)
{
    if (this != &other)
    {
        this->handle(INVALID_HANDLE_VALUE);
        this->file_path = other.file_path;

        if (other)
        {
            HANDLE handle_data = INVALID_HANDLE_VALUE;
            if (::DuplicateHandle(::GetCurrentProcess(), other.handle(), ::GetCurrentProcess(), &handle_data, 0, FALSE, DUPLICATE_SAME_ACCESS))
            {
                this->handle(handle_data);
            }

            assert(handle_data != INVALID_HANDLE_VALUE);
        }
    }

    return *this;
}

void ff::data::file_read::swap(file_read& other)
{
    std::swap(this->file_path, other.file_path);
    this->file_base::swap(other);
}

ff::data::file_read::operator bool() const
{
    return this->file_base::operator bool();
}

bool ff::data::file_read::operator!() const
{
    return this->file_base::operator!();
}

size_t ff::data::file_read::read(void* data, size_t size)
{
    DWORD read = 0;
    if (data && size && ::ReadFile(this->handle(), data, static_cast<DWORD>(size), &read, nullptr))
    {
        return static_cast<size_t>(read);
    }

    return 0;
}

const std::filesystem::path& ff::data::file_read::path() const
{
    return this->file_path;
}

ff::data::file_write::file_write(std::string_view path, bool append)
    : file_write(std::filesystem::path(path))
{
}

ff::data::file_write::file_write(const std::filesystem::path& path, bool append)
    : file_path(path)
{
    HANDLE handle_data = ::CreateFile2(ff::string::to_wstring(path.string()).c_str(), GENERIC_WRITE, FILE_SHARE_READ, append ? OPEN_ALWAYS : CREATE_ALWAYS, nullptr);
    assert(handle_data != INVALID_HANDLE_VALUE);
    this->handle(handle_data);

    if (handle_data != INVALID_HANDLE_VALUE)
    {
        this->pos(this->size());
    }
}

ff::data::file_write::file_write(file_write&& other) noexcept
{
    *this = std::move(other);
}

ff::data::file_write& ff::data::file_write::operator=(file_write&& other) noexcept
{
    if (this != &other)
    {
        this->handle(INVALID_HANDLE_VALUE);
        this->swap(other);
    }

    return *this;
}

void ff::data::file_write::swap(file_write& other)
{
    std::swap(this->file_path, other.file_path);
    this->file_base::swap(other);
}

ff::data::file_write::operator bool() const
{
    return this->file_base::operator bool();
}

bool ff::data::file_write::operator!() const
{
    return this->file_base::operator!();
}

const std::filesystem::path& ff::data::file_write::path() const
{
    return this->file_path;
}

size_t ff::data::file_write::write(const void* data, size_t size)
{
    DWORD written = 0;
    if (data && size && ::WriteFile(this->handle(), data, static_cast<DWORD>(size), &written, nullptr))
    {
        return static_cast<size_t>(written);
    }

    return 0;
}

ff::data::file_mem_mapped::file_mem_mapped(std::string_view path)
    : file_mem_mapped(file_read(path))
{
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

ff::data::file_mem_mapped& ff::data::file_mem_mapped::operator=(file_mem_mapped&& other) noexcept
{
    if (this != &other)
    {
        this->close();
        this->swap(other);
    }

    return *this;
}

ff::data::file_mem_mapped& ff::data::file_mem_mapped::operator=(const file_mem_mapped& other)
{
    if (this != &other)
    {
        this->close();
        this->mapping_file = other.mapping_file;
        this->open();
    }

    return *this;
}

void ff::data::file_mem_mapped::swap(file_mem_mapped& other)
{
    if (this != &other)
    {
        std::swap(this->mapping_file, other.mapping_file);
        std::swap(this->mapping_handle, other.mapping_handle);
        std::swap(this->mapping_size, other.mapping_size);
        std::swap(this->mapping_data, other.mapping_data);
    }
}

size_t ff::data::file_mem_mapped::size() const
{
    return this->mapping_size;
}

const uint8_t* ff::data::file_mem_mapped::data() const
{
    return this->mapping_data;
}

ff::data::file_mem_mapped::operator bool() const
{
    return this->mapping_handle && this->mapping_data;
}

bool ff::data::file_mem_mapped::operator!() const
{
    return !this->mapping_handle || !this->mapping_data;
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

const std::filesystem::path& ff::data::file_mem_mapped::path() const
{
    return this->mapping_file.path();
}

const ff::data::file_read& ff::data::file_mem_mapped::file() const
{
    return this->mapping_file;
}

void std::swap(ff::data::file_read& value1, ff::data::file_read& value2)
{
    return value1.swap(value2);
}

void std::swap(ff::data::file_write& value1, ff::data::file_write& value2)
{
    return value1.swap(value2);
}

void std::swap(ff::data::file_mem_mapped& value1, ff::data::file_mem_mapped& value2)
{
    return value1.swap(value2);
}
