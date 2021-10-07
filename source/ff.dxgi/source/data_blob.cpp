#include "pch.h"
#include "data_blob.h"

ff::dxgi::data_blob_dx::data_blob_dx(ID3DBlob* blob)
    : data_blob_dx(blob, 0, blob->GetBufferSize())
{}

ff::dxgi::data_blob_dx::data_blob_dx(ID3DBlob* blob, size_t offset, size_t size)
    : blob(blob)
    , offset(offset)
    , size_(size)
{
    assert(blob && offset + size <= blob->GetBufferSize());
}

size_t ff::dxgi::data_blob_dx::size() const
{
    return this->size_;
}

const uint8_t* ff::dxgi::data_blob_dx::data() const
{
    return reinterpret_cast<const uint8_t*>(this->blob->GetBufferPointer()) + this->offset;
}

std::shared_ptr<ff::data_base> ff::dxgi::data_blob_dx::subdata(size_t offset, size_t size) const
{
    assert(offset + size <= this->size_);
    return std::make_shared<data_blob_dx>(this->blob.Get(), this->offset + offset, size);
}

ff::dxgi::data_blob_dxtex::data_blob_dxtex(DirectX::Blob&& blob)
    : data_blob_dxtex(std::move(blob), 0, blob.GetBufferSize())
{}

ff::dxgi::data_blob_dxtex::data_blob_dxtex(DirectX::Blob&& blob, size_t offset, size_t size)
    : data_blob_dxtex(std::make_shared<DirectX::Blob>(std::move(blob)), offset, size)
{}

ff::dxgi::data_blob_dxtex::data_blob_dxtex(std::shared_ptr<DirectX::Blob> blob, size_t offset, size_t size)
    : blob(blob)
    , offset(offset)
    , size_(size)
{
    assert(offset + size <= this->blob->GetBufferSize());
}

size_t ff::dxgi::data_blob_dxtex::size() const
{
    return this->size_;
}

const uint8_t* ff::dxgi::data_blob_dxtex::data() const
{
    return reinterpret_cast<const uint8_t*>(this->blob->GetBufferPointer()) + this->offset;
}

std::shared_ptr<ff::data_base> ff::dxgi::data_blob_dxtex::subdata(size_t offset, size_t size) const
{
    assert(offset + size <= this->size_);
    return std::make_shared<data_blob_dxtex>(this->blob, this->offset + offset, size);
}
