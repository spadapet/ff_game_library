#include "pch.h"
#include "base/arena.h"
#include "base/assert.h"
#include "base/string.h"
#include "dx12/dx12_globals.h"
#include "dx12/dx12_heap.h"

ff_string_view ff_dx12_heap_usage_name(ff_dx12_heap_usage usage)
{
    switch (usage)
    {
        case ff_dx12_heap_usage_upload: return FF_SVL("upload");
        case ff_dx12_heap_usage_readback: return FF_SVL("readback");
        case ff_dx12_heap_usage_gpu_buffers: return FF_SVL("gpu_buffers");
        case ff_dx12_heap_usage_gpu_textures: return FF_SVL("gpu_textures");
        case ff_dx12_heap_usage_gpu_targets: return FF_SVL("gpu_targets");
        default: FF_DEBUG_FAIL_MSG_RET_VAL("unhandled heap usage", ff_string_view_empty());
    }
}

static D3D12_HEAP_PROPERTIES heap_properties(D3D12_HEAP_TYPE type)
{
    D3D12_HEAP_PROPERTIES props = { 0 };
    props.Type = type;
    props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    props.CreationNodeMask = 1;
    props.VisibleNodeMask = 1;
    return props;
}

bool ff_dx12_heap_valid(const ff_dx12_heap* heap)
{
    return heap && heap->heap != NULL;
}

void* ff_dx12_heap_cpu_data(const ff_dx12_heap* heap)
{
    return heap ? heap->cpu_data : NULL;
}

D3D12_GPU_VIRTUAL_ADDRESS ff_dx12_heap_gpu_data(const ff_dx12_heap* heap)
{
    return (heap && heap->cpu_resource) ? ID3D12Resource_GetGPUVirtualAddress(heap->cpu_resource) : 0;
}

uint64_t ff_dx12_heap_size(const ff_dx12_heap* heap)
{
    return heap ? heap->size : 0;
}

ff_dx12_heap_usage ff_dx12_heap_get_usage(const ff_dx12_heap* heap)
{
    return heap ? heap->usage : ff_dx12_heap_usage_upload;
}

bool ff_dx12_heap_cpu_usage(const ff_dx12_heap* heap)
{
    return heap && (heap->usage == ff_dx12_heap_usage_upload || heap->usage == ff_dx12_heap_usage_readback);
}

ff_dx12_residency_data* ff_dx12_heap_residency_data(ff_dx12_heap* heap)
{
    return heap ? &heap->residency_data : NULL;
}

bool ff_dx12_heap_init(ff_dx12_heap* heap, ff_string_view name, uint64_t size, ff_dx12_heap_usage usage)
{
    FF_ASSERT_RET_VAL(heap, false);

    *heap = (ff_dx12_heap){ 0 };
    heap->size = size;
    heap->usage = usage;

    ff_arena_declare_stack(name_arena, 256);
    ff_wstring_view wide_name = ff_utf8_to_wide(name, &name_arena, true);
    wcsncpy_s(heap->name, _countof(heap->name), wide_name.data, _TRUNCATE);
    ff_arena_destroy(&name_arena);

    D3D12_HEAP_PROPERTIES props = heap_properties(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESIDENCY_PRIORITY priority = D3D12_RESIDENCY_PRIORITY_NORMAL;
    D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE;
    uint64_t alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
    bool starts_resident = true;

    if (ff_dx12_supports_create_heap_not_resident())
    {
        flags = D3D12_HEAP_FLAG_CREATE_NOT_ZEROED | D3D12_HEAP_FLAG_CREATE_NOT_RESIDENT;
        starts_resident = false;
    }

    switch (usage)
    {
        case ff_dx12_heap_usage_upload:
            props = heap_properties(D3D12_HEAP_TYPE_UPLOAD);
            flags |= D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
            priority = D3D12_RESIDENCY_PRIORITY_LOW;
            break;

        case ff_dx12_heap_usage_readback:
            props = heap_properties(D3D12_HEAP_TYPE_READBACK);
            flags |= D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
            priority = D3D12_RESIDENCY_PRIORITY_LOW;
            break;

        case ff_dx12_heap_usage_gpu_buffers:
            flags |= D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
            break;

        case ff_dx12_heap_usage_gpu_textures:
            flags |= D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES;
            break;

        case ff_dx12_heap_usage_gpu_targets:
            flags |= D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES;
            priority = D3D12_RESIDENCY_PRIORITY_HIGH;
            alignment = D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT;
            break;

        default:
            FF_DEBUG_FAIL_RET_VAL(false);
    }

    D3D12_HEAP_DESC desc = { 0 };
    desc.SizeInBytes = size;
    desc.Properties = props;
    desc.Alignment = alignment;
    desc.Flags = flags;

    FF_ASSERT_HR_RET_VAL(ID3D12Device6_CreateHeap(ff_dx12_device(), &desc, &IID_ID3D12Heap, (void**)&heap->heap), false);

    ID3D12Pageable* pageable = (ID3D12Pageable*)heap->heap;
    ID3D12Device6_SetResidencyPriority(ff_dx12_device(), 1, &pageable, &priority);
    ID3D12Heap_SetName(heap->heap, heap->name);

    ff_dx12_residency_data_init(&heap->residency_data, name, pageable, size, starts_resident);

    if (ff_dx12_heap_cpu_usage(heap))
    {
        D3D12_RESOURCE_DESC buffer_desc = { 0 };
        buffer_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        buffer_desc.Width = size;
        buffer_desc.Height = 1;
        buffer_desc.DepthOrArraySize = 1;
        buffer_desc.MipLevels = 1;
        buffer_desc.SampleDesc.Count = 1;
        buffer_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        D3D12_RESOURCE_STATES state = (usage == ff_dx12_heap_usage_upload)
            ? D3D12_RESOURCE_STATE_GENERIC_READ
            : D3D12_RESOURCE_STATE_COPY_DEST;

        if (FAILED(ID3D12Device6_CreatePlacedResource(ff_dx12_device(), heap->heap, 0, &buffer_desc,
                state, NULL, &IID_ID3D12Resource, (void**)&heap->cpu_resource)) ||
            FAILED(ID3D12Resource_Map(heap->cpu_resource, 0, NULL, &heap->cpu_data)))
        {
            FF_DEBUG_FAIL_MSG_RET_VAL("failed to create or map cpu heap resource", false);
        }
    }

    return true;
}

void ff_dx12_heap_destroy(ff_dx12_heap* heap)
{
    FF_CHECK_RET(heap);

    if (heap->cpu_resource)
    {
        if (heap->cpu_data)
        {
            ID3D12Resource_Unmap(heap->cpu_resource, 0, NULL);
            heap->cpu_data = NULL;
        }

        ID3D12Resource_Release(heap->cpu_resource);
        heap->cpu_resource = NULL;
    }

    if (heap->heap)
    {
        ff_dx12_residency_data_destroy(&heap->residency_data);
        ID3D12Heap_Release(heap->heap);
        heap->heap = NULL;
    }

    heap->size = 0;
}
