#pragma once

#include "graphics_child_base.h"

namespace ff
{
    class buffer : public ff::internal::graphics_child_base
    {
    public:
        buffer(D3D11_BIND_FLAG type);
        buffer(D3D11_BIND_FLAG type, size_t size);
        buffer(D3D11_BIND_FLAG type, std::shared_ptr<ff::data_base> initial_data, bool writable);
        buffer(buffer&& other) noexcept = default;
        buffer(const buffer& other) = delete;
        virtual ~buffer() override;

        buffer& operator=(buffer&& other) noexcept = default;
        buffer& operator=(const buffer & other) = delete;
        operator bool() const;

        D3D11_BIND_FLAG type() const;
        size_t size() const;
        bool writable() const;
        void* map(size_t size);
        void unmap();
        bool update_discard(const void* data, size_t size);
        bool update_discard(const void* data, size_t data_size, size_t buffer_size);
        ID3D11Buffer* dx_buffer() const;

        // graphics_child_base
        virtual bool reset() override;

    private:
        buffer(D3D11_BIND_FLAG type, size_t size, std::shared_ptr<ff::data_base> initial_data, bool writable);

        std::shared_ptr<ff::data_base> initial_data;
#if DXVER == 11
        Microsoft::WRL::ComPtr<ID3D11Buffer> buffer_;
        Microsoft::WRL::ComPtr<ID3D11DeviceX> mapped_device;
#elif DXVER == 12
#endif
    };
}
