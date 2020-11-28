#pragma once

namespace ff::data
{
    class data_base;
    class file_read;
    class file_write;
    class saved_data_base;
    enum class saved_data_type;

    class stream_base
    {
    public:
        virtual ~stream_base() = 0;

        virtual size_t size() const = 0;
        virtual size_t pos() const = 0;
        virtual size_t pos(size_t new_pos) = 0;
        virtual std::shared_ptr<saved_data_base> saved_data(size_t offset, size_t size, size_t full_size, saved_data_type type) const = 0;
    };

    class reader_base : public stream_base
    {
    public:
        virtual size_t read(void* data, size_t size) = 0;
    };

    class writer_base : public stream_base
    {
    public:
        virtual size_t write(const void* data, size_t size) = 0;
    };

    class data_reader : public reader_base
    {
    public:
        data_reader(const std::shared_ptr<data_base>& data);

        virtual size_t read(void* data, size_t size) override;
        virtual size_t size() const override;
        virtual size_t pos() const override;
        virtual size_t pos(size_t new_pos) override;
        virtual std::shared_ptr<saved_data_base> saved_data(size_t offset, size_t saved_size, size_t loaded_size, saved_data_type type) const override;
    };

    class file_reader : public reader_base
    {
    public:
        file_reader(file_read&& file);

        virtual size_t read(void* data, size_t size) override;
        virtual size_t size() const override;
        virtual size_t pos() const override;
        virtual size_t pos(size_t new_pos) override;
        virtual std::shared_ptr<saved_data_base> saved_data(size_t offset, size_t size, size_t full_size, saved_data_type type) const override;
    };

    class data_writer : public writer_base
    {
    public:
        data_writer(const std::shared_ptr<std::vector<uint8_t>>& data);

        virtual size_t write(const void* data, size_t size) override;
        virtual size_t size() const override;
        virtual size_t pos() const override;
        virtual size_t pos(size_t new_pos) override;
        virtual std::shared_ptr<saved_data_base> saved_data(size_t offset, size_t size, size_t full_size, saved_data_type type) const override;
    };

    class file_writer : public writer_base
    {
    public:
        file_writer(file_write&& file);

        virtual size_t write(const void* data, size_t size) override;
        virtual size_t size() const override;
        virtual size_t pos() const override;
        virtual size_t pos(size_t new_pos) override;
        virtual std::shared_ptr<saved_data_base> saved_data(size_t offset, size_t size, size_t full_size, saved_data_type type) const override;
    };
}
