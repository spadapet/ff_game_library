#pragma once

namespace ff::data::internal
{
    class file_base
    {
    public:
        size_t size() const;
        size_t pos() const;
        size_t pos(size_t new_pos);
        HANDLE handle() const;

    protected:
        file_base();
        file_base(file_base&& other) noexcept;
        file_base(const file_base& other) = delete;
        ~file_base();

        operator bool() const;
        bool operator!() const;
        file_base& operator=(file_base&& other) noexcept;
        file_base& operator=(const file_base& other) = delete;
        void swap(file_base& other);
        void handle(HANDLE handle_data);

    private:
        HANDLE handle_data;
    };
}

namespace ff::data
{
    class file_read : public ff::data::internal::file_base
    {
    public:
        file_read(std::string_view path);
        file_read(const std::filesystem::path& path);
        file_read(file_read&& other) noexcept;
        file_read(const file_read& other);
        file_read() = delete;

        operator bool() const;
        bool operator!() const;
        file_read& operator=(file_read&& other) noexcept;
        file_read& operator=(const file_read& other);
        void swap(file_read& other);
        const std::filesystem::path& path() const;

        size_t read(void* data, size_t size);

    private:
        std::filesystem::path file_path;
    };

    class file_write : public ff::data::internal::file_base
    {
    public:
        file_write(std::string_view path, bool append = false);
        file_write(const std::filesystem::path& path, bool append = false);
        file_write(file_write&& other) noexcept;
        file_write(const file_write& other) = delete;
        file_write() = delete;

        operator bool() const;
        bool operator!() const;
        file_write& operator=(file_write&& other) noexcept;
        file_write& operator=(const file_write& other) = delete;
        void swap(file_write& other);
        const std::filesystem::path& path() const;

        size_t write(const void* data, size_t size);

    private:
        std::filesystem::path file_path;
    };

    class file_mem_mapped
    {
    public:
        file_mem_mapped(std::string_view path);
        file_mem_mapped(const std::filesystem::path& path);
        file_mem_mapped(const file_read& file);
        file_mem_mapped(const file_mem_mapped& other);
        file_mem_mapped(file_read&& file) noexcept;
        file_mem_mapped(file_mem_mapped&& other) noexcept;
        file_mem_mapped() = delete;
        ~file_mem_mapped();

        operator bool() const;
        bool operator!() const;
        file_mem_mapped& operator=(file_mem_mapped&& other) noexcept;
        file_mem_mapped& operator=(const file_mem_mapped& other);
        void swap(file_mem_mapped& other);

        const file_read& file() const;
        size_t size() const;
        const uint8_t* data() const;
        const std::filesystem::path& path() const;

    private:
        void open();
        void close();

        file_read mapping_file;
        HANDLE mapping_handle;
        size_t mapping_size;
        const uint8_t* mapping_data;
    };
}

namespace std
{
    void swap(ff::data::file_read& value1, ff::data::file_read& value2);
    void swap(ff::data::file_write& value1, ff::data::file_write& value2);
    void swap(ff::data::file_mem_mapped& value1, ff::data::file_mem_mapped& value2);
}
