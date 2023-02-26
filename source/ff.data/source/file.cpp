#include "pch.h"
#include "file.h"

ff::file_base::file_base(const std::filesystem::path& path)
    : path_(path)
{}

ff::file_base::file_base(file_base&& other) noexcept
    : path_(std::move(other.path_))
    , handle_(std::move(other.handle_))
{
}

size_t ff::file_base::size() const
{
    assert(this->handle_);

    FILE_STANDARD_INFO info;
    if (!::GetFileInformationByHandleEx(this->handle_, FileStandardInfo, &info, sizeof(info)))
    {
        assert(false);
        return 0;
    }

    return static_cast<size_t>(info.EndOfFile.QuadPart);
}

size_t ff::file_base::pos() const
{
    assert(this->handle_);

    LARGE_INTEGER cur{}, move{};
    if (!::SetFilePointerEx(this->handle_, move, &cur, FILE_CURRENT))
    {
        assert(false);
        return 0;
    }

    return static_cast<size_t>(cur.QuadPart);
}

size_t ff::file_base::pos(size_t new_pos)
{
    assert(this->handle_);

    LARGE_INTEGER cur{}, move{};
    move.QuadPart = new_pos;
    if (!::SetFilePointerEx(this->handle_, move, &cur, FILE_BEGIN))
    {
        assert(false);
        return 0;
    }

    return static_cast<size_t>(cur.QuadPart);
}

const ff::win_handle& ff::file_base::handle() const
{
    return this->handle_;
}

const std::filesystem::path& ff::file_base::path() const
{
    return this->path_;
}

ff::file_base::operator bool() const
{
    return this->handle_;
}

bool ff::file_base::operator!() const
{
    return !this->handle_;
}

ff::file_base& ff::file_base::operator=(file_base&& other) noexcept
{
    if (this != &other)
    {
        this->path_.clear();

        std::swap(this->handle_, other.handle_);
        std::swap(this->path_, other.path_);
    }

    return *this;
}

ff::file_base& ff::file_base::operator=(const file_base& other)
{
    if (this != &other)
    {
        this->path_ = other.path_;
        this->handle_ = other.handle_.duplicate();
    }

    return *this;
}

void ff::file_base::handle(win_handle&& file_handle)
{
    this->handle_ = std::move(file_handle);
}

ff::file_read::file_read(const std::filesystem::path& path)
    : file_base(path)
{
    win_handle file_handle(::CreateFile2(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, nullptr));
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
    if (*this && size && ::ReadFile(this->handle(), data, static_cast<DWORD>(size), &read, nullptr))
    {
        return static_cast<size_t>(read);
    }

    return 0;
}

ff::co_task<size_t> ff::file_read::read_async(void* data, size_t size)
{
    return {};
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

    win_handle file_handle(::CreateFile2(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, append ? OPEN_ALWAYS : CREATE_ALWAYS, nullptr));
    assert(file_handle);
    this->handle(std::move(file_handle));

    if (*this && append)
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
    if (*this && size && ::WriteFile(this->handle(), data, static_cast<DWORD>(size), &written, nullptr))
    {
        return static_cast<size_t>(written);
    }

    return 0;
}

ff::file_mem_mapped::file_mem_mapped(const std::filesystem::path& path)
    : file_mem_mapped(file_read(path))
{}

ff::file_mem_mapped::file_mem_mapped(file_read&& file) noexcept
    : file_(std::move(file))
    , size_(0)
    , data_(nullptr)
{
    this->open();
}

ff::file_mem_mapped::file_mem_mapped(file_mem_mapped&& other) noexcept
    : file_(std::move(other.file_))
    , handle_(std::move(other.handle_))
    , size_(other.size_)
    , data_(other.data_)
{
    other.size_ = 0;
    other.data_ = nullptr;
}

ff::file_mem_mapped::file_mem_mapped(const file_read& file)
    : file_mem_mapped(file_read(file))
{}

ff::file_mem_mapped::file_mem_mapped(const file_mem_mapped& other)
    : file_mem_mapped(other.file_)
{}

ff::file_mem_mapped::~file_mem_mapped()
{
    this->close();
}

ff::file_mem_mapped::operator bool() const
{
    return this->handle_ && this->data_;
}

bool ff::file_mem_mapped::operator!() const
{
    return !this->handle_ || !this->data_;
}

ff::file_mem_mapped& ff::file_mem_mapped::operator=(file_mem_mapped&& other) noexcept
{
    if (this != &other)
    {
        this->close();

        std::swap(this->file_, other.file_);
        std::swap(this->handle_, other.handle_);
        std::swap(this->size_, other.size_);
        std::swap(this->data_, other.data_);
    }

    return *this;
}

size_t ff::file_mem_mapped::size() const
{
    return this->size_;
}

const uint8_t* ff::file_mem_mapped::data() const
{
    return this->data_;
}

const std::filesystem::path& ff::file_mem_mapped::path() const
{
    return this->file_.path();
}

const ff::file_read& ff::file_mem_mapped::file() const
{
    return this->file_;
}

void ff::file_mem_mapped::open()
{
    this->close();

    if (this->file_ && this->file_.size())
    {
#if UWP_APP
        this->handle_ = win_handle(::CreateFileMappingFromApp(this->file_.handle(), nullptr, PAGE_READONLY, 0, nullptr));
#else
        this->handle_ = win_handle(::CreateFileMapping(this->file_.handle(), nullptr, PAGE_READONLY, 0, 0, nullptr));
#endif
        assert(this->handle_);

        if (this->handle_)
        {
            this->size_ = this->file_.size();
#if UWP_APP
            this->data_ = reinterpret_cast<const uint8_t*>(::MapViewOfFileFromApp(this->handle_, FILE_MAP_READ, 0, this->size_));
#else
            this->data_ = reinterpret_cast<const uint8_t*>(::MapViewOfFile(this->handle_, FILE_MAP_READ, 0, 0, this->size_));
#endif
            assert(this->data_);
        }
    }
}

void ff::file_mem_mapped::close()
{
    if (this->data_)
    {
        ::UnmapViewOfFile(this->data_);
        this->data_ = nullptr;
        this->size_ = 0;
    }

    this->handle_.close();
}
