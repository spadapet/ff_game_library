#pragma once

namespace ff::dx12
{
    class commands;
    class resource;

    class buffer : public ff::dxgi::buffer_base, private ff::dxgi::device_child_base
    {
    public:
        buffer(ff::dxgi::buffer_type type, std::shared_ptr<ff::data_base> initial_data = {});
        buffer(buffer&& other) noexcept = default;
        buffer(const buffer& other) = delete;
        virtual ~buffer() override;

        static ff::dx12::buffer& get(ff::dxgi::buffer_base& obj);
        static const ff::dx12::buffer& get(const ff::dxgi::buffer_base& obj);
        buffer& operator=(buffer&& other) noexcept = default;
        buffer& operator=(const buffer & other) = delete;
        operator bool() const;

        ff::dx12::resource* resource();

        // buffer_base
        virtual ff::dxgi::buffer_type type() const override;
        virtual size_t size() const override;
        virtual bool writable() const override;
        virtual bool update(ff::dxgi::command_context_base& context, const void* data, size_t size, size_t min_buffer_size = 0) override;
        virtual void* map(ff::dxgi::command_context_base& context, size_t size) override;
        virtual void unmap() override;

    private:
        buffer(ff::dx12::commands* commands, ff::dxgi::buffer_type type, const void* data, uint64_t data_size, std::shared_ptr<ff::data_base> initial_data);

        // device_child_base
        virtual bool reset() override;

        std::unique_ptr<ff::dx12::resource> resource_;
        std::shared_ptr<ff::data_base> initial_data;
        std::unique_ptr<std::vector<uint8_t>> mapped_mem;
        ff::dxgi::command_context_base* mapped_context;
        ff::dxgi::buffer_type type_;
    };
}
