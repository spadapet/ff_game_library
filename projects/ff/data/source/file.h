#pragma once

namespace ff::data
{
    class file_base
    {
    protected:
        file_base();
        file_base(const std::filesystem::path& path);
        file_base(file_base&& other) noexcept;
        file_base(const file_base& other) = delete;
        virtual ~file_base() = 0;

    public:
        size_t size() const;
        size_t pos() const;
        size_t pos(size_t new_pos);
        HANDLE handle() const;
        const std::filesystem::path& path() const;

        operator bool() const;
        bool operator!() const;

    protected:
        file_base& operator=(file_base&& other) noexcept;
        file_base& operator=(const file_base& other);
        void handle(HANDLE handle_data);

    private:
        std::filesystem::path file_path;
        HANDLE file_handle;
    };

    class file_read : public file_base
    {
    public:
        file_read(const std::filesystem::path& path);
        file_read(file_read&& other) noexcept;
        file_read(const file_read& other);
        file_read() = delete;

        file_read& operator=(file_read&& other) noexcept;
        file_read& operator=(const file_read& other);
        const std::filesystem::path& path() const;

        size_t read(void* data, size_t size);
    };

    class file_write : public file_base
    {
    public:
        file_write(const std::filesystem::path& path, bool append = false);
        file_write(file_write&& other) noexcept;
        file_write(const file_write& other) = delete;
        file_write() = delete;

        file_write& operator=(file_write&& other) noexcept;
        file_write& operator=(const file_write& other) = delete;

        size_t write(const void* data, size_t size);
    };

    class file_mem_mapped
    {
    public:
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
        file_mem_mapped& operator=(const file_mem_mapped& other) = delete;

        size_t size() const;
        const uint8_t* data() const;
        const std::filesystem::path& path() const;
        const file_read& file() const;

    private:
        void open();
        void close();

        file_read mapping_file;
        HANDLE mapping_handle;
        size_t mapping_size;
        const uint8_t* mapping_data;
    };
}
