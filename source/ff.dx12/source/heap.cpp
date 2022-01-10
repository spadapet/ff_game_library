#include "pch.h"
#include "device_reset_priority.h"
#include "globals.h"
#include "heap.h"

// Don't create resident or zeroed:
// https://devblogs.microsoft.com/directx/coming-to-directx-12-more-control-over-memory-allocation/

std::string_view ff::dx12::heap::usage_name(ff::dx12::heap::usage_t usage)
{
    switch (usage)
    {
        default: debug_fail_ret_val("");
        case ff::dx12::heap::usage_t::upload: return "upload";
        case ff::dx12::heap::usage_t::readback: return "readback";
        case ff::dx12::heap::usage_t::gpu_buffers: return "gpu_buffers";
        case ff::dx12::heap::usage_t::gpu_textures: return "gpu_textures";
        case ff::dx12::heap::usage_t::gpu_targets: return "gpu_targets";
    }

    return std::string_view();
}

ff::dx12::heap::heap(std::string_view name, uint64_t size, ff::dx12::heap::usage_t usage)
    : name_(name)
    , cpu_data_(nullptr)
    , size_(size)
    , usage_(usage)
{
    this->reset();
    ff::dx12::add_device_child(this, ff::dx12::device_reset_priority::heap);
}

ff::dx12::heap::heap(heap&& other) noexcept
{
    *this = std::move(other);
    ff::dx12::add_device_child(this, ff::dx12::device_reset_priority::heap);
}

ff::dx12::heap::~heap()
{
    this->cpu_unmap();
    ff::dx12::remove_device_child(this);
}

ff::dx12::heap::operator bool() const
{
    return this->heap_ != nullptr;
}

const std::string& ff::dx12::heap::name() const
{
    return this->name_;
}

void* ff::dx12::heap::cpu_data()
{
    if (this->heap_ && this->cpu_usage())
    {
        if (!this->cpu_data_)
        {
            Microsoft::WRL::ComPtr<ID3D12Resource> cpu_resource;

            if (!this->cpu_resource && FAILED(ff::dx12::device()->CreatePlacedResource(
                this->heap_.Get(),
                0, // start
                &CD3DX12_RESOURCE_DESC::Buffer(this->size_),
                (this->usage_ == ff::dx12::heap::usage_t::upload) ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr, // clear value
                IID_PPV_ARGS(&cpu_resource))))
            {
                assert(false);
                return nullptr;
            }

            if (FAILED(cpu_resource.As(&this->cpu_resource)) ||
                FAILED(this->cpu_resource->Map(0, nullptr, &this->cpu_data_)))
            {
                assert(false);
                return nullptr;
            }
        }

        return this->cpu_data_;
    }

    return nullptr;
}

D3D12_GPU_VIRTUAL_ADDRESS ff::dx12::heap::gpu_data()
{
    return this->cpu_data() ? this->cpu_resource->GetGPUVirtualAddress() : 0;
}

uint64_t ff::dx12::heap::size() const
{
    return this->size_;
}

ff::dx12::heap::usage_t ff::dx12::heap::usage() const
{
    return this->usage_;
}

bool ff::dx12::heap::cpu_usage() const
{
    return this->usage_ == ff::dx12::heap::usage_t::upload || this->usage_ == ff::dx12::heap::usage_t::readback;
}

ff::dx12::residency_data* ff::dx12::heap::residency_data()
{
    return this->residency_data_.get();
}

void ff::dx12::heap::before_reset()
{
    this->cpu_unmap();
    this->residency_data_.reset();
    this->heap_.Reset();
}

bool ff::dx12::heap::reset()
{
    D3D12_HEAP_PROPERTIES props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESIDENCY_PRIORITY priority = D3D12_RESIDENCY_PRIORITY_NORMAL;
    D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE;
    bool starts_resident = true;

    if (ff::dx12::supports_create_heap_not_resident())
    {
        flags = D3D12_HEAP_FLAG_CREATE_NOT_ZEROED | D3D12_HEAP_FLAG_CREATE_NOT_RESIDENT;
        starts_resident = false;
    }

    switch (this->usage_)
    {
        default:
            assert(false);
            return false;

        case ff::dx12::heap::usage_t::upload:
            props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            priority = D3D12_RESIDENCY_PRIORITY_LOW;
            break;

        case ff::dx12::heap::usage_t::readback:
            props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
            priority = D3D12_RESIDENCY_PRIORITY_LOW;
            break;

        case ff::dx12::heap::usage_t::gpu_buffers:
            flags |= D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS | D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES | D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES;
            break;

        case ff::dx12::heap::usage_t::gpu_textures:
            flags |= D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES | D3D12_HEAP_FLAG_DENY_BUFFERS | D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES;
            break;

        case ff::dx12::heap::usage_t::gpu_targets:
            flags |= D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES | D3D12_HEAP_FLAG_DENY_BUFFERS | D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES;
            priority = D3D12_RESIDENCY_PRIORITY_HIGH;
            break;
    }

    CD3DX12_HEAP_DESC desc(this->size_, props, 0, flags);
    if (SUCCEEDED(ff::dx12::device()->CreateHeap(&desc, IID_PPV_ARGS(&this->heap_))))
    {
        this->heap_->SetName(ff::string::to_wstring(this->name_).c_str());

        ID3D12Pageable* p = this->heap_.Get();
        ff::dx12::device()->SetResidencyPriority(1, &p, &priority);

        this->residency_data_ = std::make_unique<ff::dx12::residency_data>(p, this->size_, starts_resident);

        return true;
    }

    ff::dx12::device_fatal_error("Heap creation failure (out of memory)");
    return false;
}

void ff::dx12::heap::cpu_unmap()
{
    if (this->cpu_resource)
    {
        if (this->cpu_data_)
        {
            this->cpu_resource->Unmap(0, nullptr);
            this->cpu_data_ = nullptr;
        }

        this->cpu_resource.Reset();
    }
}
