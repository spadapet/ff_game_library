#pragma once

namespace ff::dx11
{
    class buffer : public ff::dxgi::buffer_base, private ff::dxgi::device_child_base
    {
    public:
        buffer(ff::dxgi::buffer_type type);
        buffer(ff::dxgi::buffer_type type, size_t size);
        buffer(ff::dxgi::buffer_type type, std::shared_ptr<ff::data_base> initial_data, bool writable);
        buffer(buffer&& other) noexcept;
        buffer(const buffer& other) = delete;
        virtual ~buffer() override;

        static ff::dx11::buffer& get(ff::dxgi::buffer_base& obj);
        static const ff::dx11::buffer& get(const ff::dxgi::buffer_base& obj);
        buffer& operator=(buffer&& other) noexcept = default;
        buffer& operator=(const buffer & other) = delete;
        operator bool() const;

        ID3D11Buffer* dx_buffer() const;

        // buffer_base
        virtual ff::dxgi::buffer_type type() const override;
        virtual size_t size() const override;
        virtual bool writable() const override;
        virtual bool update(ff::dxgi::command_context_base& context, const void* data, size_t size, size_t min_buffer_size = 0) override;
        virtual void* map(ff::dxgi::command_context_base& context, size_t size) override;
        virtual void unmap() override;

    private:
        buffer(ff::dxgi::buffer_type type, size_t size, std::shared_ptr<ff::data_base> initial_data, bool writable);

        // device_child_base
        virtual bool reset() override;

        Microsoft::WRL::ComPtr<ID3D11Buffer> buffer_;
        std::shared_ptr<ff::data_base> initial_data;
        ff::dxgi::command_context_base* mapped_context;
    };
}
