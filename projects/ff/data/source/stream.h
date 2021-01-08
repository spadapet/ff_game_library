#pragma once

#include "file.h"

namespace ff
{
    class data_base;
    class saved_data_base;
    enum class saved_data_type;

    class stream_base
    {
    public:
        virtual ~stream_base() = 0;

        virtual size_t size() const = 0;
        virtual size_t pos() const = 0;
        virtual size_t pos(size_t new_pos) = 0;
        virtual std::shared_ptr<saved_data_base> saved_data(size_t offset, size_t saved_size, size_t loaded_size, saved_data_type type) const = 0;
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
        data_reader(data_reader&& other) noexcept = default;
        data_reader(const data_reader& other) = delete;

        data_reader& operator=(data_reader&& other) noexcept = default;
        data_reader& operator=(const data_reader& other) = delete;

        virtual size_t read(void* data, size_t size) override;
        virtual size_t size() const override;
        virtual size_t pos() const override;
        virtual size_t pos(size_t new_pos) override;
        virtual std::shared_ptr<saved_data_base> saved_data(size_t offset, size_t saved_size, size_t loaded_size, saved_data_type type) const override;

    private:
        std::shared_ptr<data_base> data;
        size_t data_pos;
    };

    class file_reader : public reader_base
    {
    public:
        file_reader(file_read&& file);
        file_reader(file_reader&& other) noexcept = default;
        file_reader(const std::filesystem::path& path);
        file_reader(const file_reader& other) = delete;

        file_reader& operator=(file_reader&& other) noexcept = default;
        file_reader& operator=(const file_reader& other) = delete;
        operator bool() const;
        bool operator!() const;

        virtual size_t read(void* data, size_t size) override;
        virtual size_t size() const override;
        virtual size_t pos() const override;
        virtual size_t pos(size_t new_pos) override;
        virtual std::shared_ptr<saved_data_base> saved_data(size_t offset, size_t saved_size, size_t loaded_size, saved_data_type type) const override;

    private:
        file_read file;
    };

    class data_writer : public writer_base
    {
    public:
        data_writer(const std::shared_ptr<std::vector<uint8_t>>& data);
        data_writer(const std::shared_ptr<std::vector<uint8_t>>& data, size_t pos);
        data_writer(data_writer&& other) noexcept = default;
        data_writer(const data_writer& other) = delete;

        data_writer& operator=(data_writer&& other) noexcept = default;
        data_writer& operator=(const data_writer& other) = delete;

        virtual size_t write(const void* data, size_t size) override;
        virtual size_t size() const override;
        virtual size_t pos() const override;
        virtual size_t pos(size_t new_pos) override;
        virtual std::shared_ptr<saved_data_base> saved_data(size_t offset, size_t saved_size, size_t loaded_size, saved_data_type type) const override;

    private:
        std::shared_ptr<std::vector<uint8_t>> data;
        size_t data_pos;
    };

    class file_writer : public writer_base
    {
    public:
        file_writer(file_write&& file);
        file_writer(const std::filesystem::path& path, bool append = false);
        file_writer(file_writer&& other) noexcept = default;
        file_writer(const file_writer& other) = delete;

        file_writer& operator=(file_writer&& other) noexcept = default;
        file_writer& operator=(const file_writer& other) = delete;
        operator bool() const;
        bool operator!() const;

        virtual size_t write(const void* data, size_t size) override;
        virtual size_t size() const override;
        virtual size_t pos() const override;
        virtual size_t pos(size_t new_pos) override;
        virtual std::shared_ptr<saved_data_base> saved_data(size_t offset, size_t saved_size, size_t loaded_size, saved_data_type type) const override;

    private:
        file_write file;
    };

    size_t stream_copy(writer_base& writer, reader_base& reader, size_t size, size_t chunk_size = 0);
    Microsoft::WRL::ComPtr<IStream> get_stream(const std::shared_ptr<reader_base>& reader);
}
