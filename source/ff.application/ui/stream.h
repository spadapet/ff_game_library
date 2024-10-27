#pragma once

namespace ff::internal::ui
{
    class stream : public Noesis::Stream
    {
    public:
        stream(ff::auto_resource_value&& resource);
        stream(std::shared_ptr<ff::reader_base> reader);
        stream(stream&& other) noexcept = default;
        stream(const stream& other) = delete;

        stream& operator=(stream&& other) noexcept = default;
        stream& operator=(const stream & other) = delete;

        virtual void SetPosition(uint32_t pos) override;
        virtual uint32_t GetPosition() const override;
        virtual uint32_t GetLength() const override;
        virtual uint32_t Read(void* buffer, uint32_t size) override;
        virtual const void* GetMemoryBase() const override;
        virtual void Close() override;

    private:
        ff::auto_resource_value resource;
        std::shared_ptr<ff::reader_base> reader;
        std::shared_ptr<ff::data_base> data;
    };
}
