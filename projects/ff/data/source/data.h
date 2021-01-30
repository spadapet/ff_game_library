#pragma once

namespace ff
{
    class file_mem_mapped;

    class data_base
    {
    public:
        virtual ~data_base() = 0;

        virtual size_t size() const = 0;
        virtual const uint8_t* data() const = 0;
        virtual std::shared_ptr<data_base> subdata(size_t offset, size_t size) const = 0;
    };

    class data_static : public data_base
    {
    public:
        data_static(const void* data, size_t size);
#if !UWP_APP
        data_static(HINSTANCE instance, const wchar_t* rc_type, const wchar_t* rc_name);
#endif
        data_static(const data_static& other) = default;

        data_static& operator=(const data_static& other) = default;

        virtual size_t size() const override;
        virtual const uint8_t* data() const override;
        virtual std::shared_ptr<data_base> subdata(size_t offset, size_t size) const override;

    private:
        const uint8_t* data_;
        size_t size_;
    };

    class data_mem_mapped : public data_base
    {
    public:
        data_mem_mapped(const std::shared_ptr<file_mem_mapped>& file);
        data_mem_mapped(const std::shared_ptr<file_mem_mapped>& file, size_t offset, size_t size);
        data_mem_mapped(const std::filesystem::path& path);
        data_mem_mapped(file_mem_mapped&& file) noexcept;
        data_mem_mapped(file_mem_mapped&& file, size_t offset, size_t size) noexcept;
        data_mem_mapped(const data_mem_mapped& other) = default;
        data_mem_mapped(data_mem_mapped&& other) noexcept = default;

        data_mem_mapped& operator=(const data_mem_mapped& other) = default;
        data_mem_mapped& operator=(data_mem_mapped&& other) noexcept = default;

        bool valid() const;
        const std::shared_ptr<file_mem_mapped>& file() const;
        size_t offset() const;

        virtual size_t size() const override;
        virtual const uint8_t* data() const override;
        virtual std::shared_ptr<data_base> subdata(size_t offset, size_t size) const override;

    private:
        std::shared_ptr<file_mem_mapped> file_;
        size_t offset_;
        size_t size_;
    };

    class data_vector : public data_base
    {
    public:
        data_vector(const std::shared_ptr<const std::vector<uint8_t>>& vector);
        data_vector(const std::shared_ptr<const std::vector<uint8_t>>& vector, size_t offset, size_t size);
        data_vector(std::vector<uint8_t>&& vector) noexcept;
        data_vector(std::vector<uint8_t>&& vector, size_t offset, size_t size) noexcept;
        data_vector(const data_vector& other) = default;
        data_vector(data_vector&& other) noexcept = default;

        data_vector& operator=(const data_vector& other) = default;
        data_vector& operator=(data_vector&& other) noexcept = default;

        const std::shared_ptr<const std::vector<uint8_t>>& vector() const;
        size_t offset() const;

        virtual size_t size() const override;
        virtual const uint8_t* data() const override;
        virtual std::shared_ptr<data_base> subdata(size_t offset, size_t size) const override;

    private:
        std::shared_ptr<const std::vector<uint8_t>> vector_;
        size_t offset_;
        size_t size_;
    };
}
