#pragma once

namespace ff::internal
{
    class data_blob_dx : public data_base
    {
    public:
        data_blob_dx(ID3DBlob* blob);
        data_blob_dx(ID3DBlob* blob, size_t offset, size_t size);
        data_blob_dx(const data_blob_dx& other) = default;
        data_blob_dx(data_blob_dx&& other) noexcept = default;

        data_blob_dx& operator=(const data_blob_dx& other) = default;
        data_blob_dx& operator=(data_blob_dx&& other) noexcept = default;

        virtual size_t size() const override;
        virtual const uint8_t* data() const override;
        virtual std::shared_ptr<data_base> subdata(size_t offset, size_t size) const override;

    private:
        Microsoft::WRL::ComPtr<ID3DBlob> blob;
        size_t offset;
        size_t size_;
    };

    class data_blob_dxtex : public data_base
    {
    public:
        data_blob_dxtex(DirectX::Blob&& blob);
        data_blob_dxtex(DirectX::Blob&& blob, size_t offset, size_t size);
        data_blob_dxtex(std::shared_ptr<DirectX::Blob> blob, size_t offset, size_t size);
        data_blob_dxtex(const data_blob_dxtex& other) = default;
        data_blob_dxtex(data_blob_dxtex&& other) noexcept = default;

        data_blob_dxtex& operator=(const data_blob_dxtex& other) = default;
        data_blob_dxtex& operator=(data_blob_dxtex&& other) noexcept = default;

        virtual size_t size() const override;
        virtual const uint8_t* data() const override;
        virtual std::shared_ptr<data_base> subdata(size_t offset, size_t size) const override;

    private:
        std::shared_ptr<DirectX::Blob> blob;
        size_t offset;
        size_t size_;
    };
}
