#pragma once

namespace ff::data
{
    class file_mem_mapped;

    class data_base
    {
    public:
        virtual ~data_base() = 0;

        virtual size_t size() const = 0;
        virtual const uint8_t* data() const = 0;
        virtual std::unique_ptr<data_base> subdata(size_t offset, size_t size) const = 0;
    };

    class data_static : public data_base
    {
    public:
        data_static(const void* data, size_t size);
        data_static(const data_static& other);

        data_static& operator=(const data_static& other);
        void swap(data_static& other);

        virtual size_t size() const override;
        virtual const uint8_t* data() const override;
        virtual std::unique_ptr<data_base> subdata(size_t offset, size_t size) const override;

    private:
        const uint8_t* static_data;
        size_t data_size;
    };

    class data_mem_mapped : public data_base
    {
    public:
        data_mem_mapped(const std::shared_ptr<ff::data::file_mem_mapped>& file);
        data_mem_mapped(const std::shared_ptr<ff::data::file_mem_mapped>& file, size_t offset, size_t size);
        data_mem_mapped(const data_mem_mapped& other);
        data_mem_mapped(data_mem_mapped&& other) noexcept;

        data_mem_mapped& operator=(const data_mem_mapped& other);
        data_mem_mapped& operator=(data_mem_mapped&& other) noexcept;
        void swap(data_mem_mapped& other);

        const std::shared_ptr<ff::data::file_mem_mapped>& file() const;
        size_t offset() const;

        virtual size_t size() const override;
        virtual const uint8_t* data() const override;
        virtual std::unique_ptr<data_base> subdata(size_t offset, size_t size) const override;

    private:
        std::shared_ptr<ff::data::file_mem_mapped> shared_file;
        size_t data_offset;
        size_t data_size;
    };

    class data_vector : public data_base
    {
    public:
        data_vector(const std::shared_ptr<const std::vector<uint8_t>>& vector);
        data_vector(const std::shared_ptr<const std::vector<uint8_t>>& vector, size_t offset, size_t size);
        data_vector(const data_vector& other);
        data_vector(data_vector&& other) noexcept;

        data_vector& operator=(const data_vector& other);
        data_vector& operator=(data_vector&& other) noexcept;
        void swap(data_vector& other);

        const std::shared_ptr<const std::vector<uint8_t>>& vector() const;
        size_t offset() const;

        virtual size_t size() const override;
        virtual const uint8_t* data() const override;
        virtual std::unique_ptr<data_base> subdata(size_t offset, size_t size) const override;

    private:
        std::shared_ptr<const std::vector<uint8_t>> shared_vector;
        size_t data_offset;
        size_t data_size;
    };
}

namespace std
{
    void swap(ff::data::data_static& value1, ff::data::data_static& value2);
    void swap(ff::data::data_mem_mapped& value1, ff::data::data_mem_mapped& value2);
    void swap(ff::data::data_vector& value1, ff::data::data_vector& value2);
}
