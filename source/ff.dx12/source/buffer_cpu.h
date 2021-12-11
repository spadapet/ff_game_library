#pragma once

namespace ff::dx12
{
    class buffer_cpu : public ff::dxgi::buffer_base
    {
    public:
        buffer_cpu(ff::dxgi::buffer_type type);
        buffer_cpu(buffer_cpu&& other) noexcept = default;
        buffer_cpu(const buffer_cpu& other) = delete;

        buffer_cpu& operator=(buffer_cpu&& other) noexcept = default;
        buffer_cpu& operator=(const buffer_cpu& other) = delete;
        operator bool() const;

        const std::vector<uint8_t>& data() const;
        size_t version() const;

        // buffer_base
        virtual ff::dxgi::buffer_type type() const override;
        virtual size_t size() const override;
        virtual bool writable() const override;
        virtual bool update(ff::dxgi::command_context_base& context, const void* data, size_t size, size_t min_buffer_size = 0) override;
        virtual void* map(ff::dxgi::command_context_base& context, size_t size) override;
        virtual void unmap() override;

    private:
        std::vector<uint8_t> data_;
        ff::dxgi::buffer_type type_;
        size_t version_;
    };
}
