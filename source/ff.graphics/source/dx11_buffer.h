#pragma once

#include "graphics_child_base.h"

#if DXVER == 11

namespace ff
{
    class dx11_buffer : public ff::internal::graphics_child_base
    {
    public:
        dx11_buffer(D3D11_BIND_FLAG type);
        dx11_buffer(D3D11_BIND_FLAG type, size_t size);
        dx11_buffer(D3D11_BIND_FLAG type, std::shared_ptr<ff::data_base> initial_data, bool writable);
        dx11_buffer(dx11_buffer&& other) noexcept = default;
        dx11_buffer(const dx11_buffer& other) = delete;
        virtual ~dx11_buffer() override;

        dx11_buffer& operator=(dx11_buffer&& other) noexcept = default;
        dx11_buffer& operator=(const dx11_buffer & other) = delete;
        operator bool() const;

        D3D11_BIND_FLAG type() const;
        ID3D11Buffer* dx_buffer() const;
        size_t size() const;
        bool writable() const;
        void* map(size_t size);
        void unmap();
        bool update_discard(const void* data, size_t size);
        bool update_discard(const void* data, size_t data_size, size_t buffer_size);

        // graphics_child_base
        virtual bool reset() override;

    private:
        dx11_buffer(D3D11_BIND_FLAG type, size_t size, std::shared_ptr<ff::data_base> initial_data, bool writable);

        Microsoft::WRL::ComPtr<ID3D11Buffer> buffer_;
        Microsoft::WRL::ComPtr<ID3D11DeviceX> mapped_device;
        std::shared_ptr<ff::data_base> initial_data;
    };
}

#endif
