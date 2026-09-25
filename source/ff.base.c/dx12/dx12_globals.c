#include "pch.h"
#include "base/arena.h"
#include "base/assert.h"
#include "base/hash.h"
#include "base/log.h"
#include "base/math.h"
#include "base/string.h"
#include "dx12/dx12_globals.h"
#include "dx12/dx12_descriptor_allocator.h"
#include "dx12/dx12_device_child.h"
#include "dx12/dx12_mem_allocator.h"
#include "dx12/dx12_mem_range.h"
#include "dx12/dx12_queue.h"
#include "dx12/dx12_reset.h"
#include "dx12/dx12_resource.h"
#include "dx12/dx12_residency.h"

// Embedding the link dependencies here keeps them with the code that needs them, and they
// automatically flow to anything that links this static library.
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

// Max adapters enumerated in one pass. Way more than any real machine has, and it keeps
// adapter handling on the stack instead of needing a dynamic array.
#define MAX_ADAPTERS 16

typedef enum ff_dx12_mem_allocator_index
{
    ff_dx12_mem_allocator_upload,
    ff_dx12_mem_allocator_readback,
    ff_dx12_mem_allocator_dynamic_buffer,
    ff_dx12_mem_allocator_static_buffer,
    ff_dx12_mem_allocator_texture,
    ff_dx12_mem_allocator_target,
    ff_dx12_mem_allocator_count,
} ff_dx12_mem_allocator_index;

static IDXGIFactory6* s_factory;
static IDXGIAdapter3* s_adapter;
static ID3D12Device6* s_device;
static DXGI_GPU_PREFERENCE s_gpu_preference;
static D3D_FEATURE_LEVEL s_feature_level;
static uint64_t s_adapters_hash;
static bool s_supports_create_heap_not_resident;
static bool s_supports_bindless;
static bool s_simulate_device_invalid;

static HANDLE s_video_memory_change_event;
static DWORD s_video_memory_change_cookie;
static DXGI_QUERY_VIDEO_MEMORY_INFO s_video_memory_info;

static ff_dx12_queue s_direct_queue;
static ff_dx12_queue s_copy_queue;
static ff_dx12_queue s_compute_queue;
static uint64_t s_frame_count;

// Resources whose GPU work hasn't retired yet. Nodes are arena-allocated and recycled through
// s_keep_alive_free, so the list never grows without bound as long as it keeps getting drained.
typedef struct ff_dx12_keep_alive_node
{
    struct ff_dx12_keep_alive_node* next;
    ID3D12Resource* resource;
    ff_dx12_mem_range mem_range;
    ff_dx12_fence_values fence_values;
} ff_dx12_keep_alive_node;

static ff_arena s_keep_alive_arena;
static bool s_keep_alive_arena_valid;
static ff_dx12_keep_alive_node* s_keep_alive_head;
static ff_dx12_keep_alive_node* s_keep_alive_tail;
static ff_dx12_keep_alive_node* s_keep_alive_free;

// Shared allocators, created on first use. Each has an explicit valid flag because a zeroed
// allocator is indistinguishable from an initialized one that hasn't allocated anything yet.
#define FF_DX12_ONE_MEG (1024ull * 1024ull)

static ff_dx12_mem_allocator s_mem_allocators[ff_dx12_mem_allocator_count];
static bool s_mem_allocator_valid[ff_dx12_mem_allocator_count];

static ff_dx12_cpu_descriptor_allocator s_cpu_descriptor_allocators[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
static bool s_cpu_descriptor_allocator_valid[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

// Only CBV_SRV_UAV and SAMPLER can be shader visible, so the other two slots stay unused.
static ff_dx12_gpu_descriptor_allocator s_gpu_descriptor_allocators[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
static bool s_gpu_descriptor_allocator_valid[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

static bool debug_layer_wanted(void)
{
#ifdef _DEBUG
    return IsDebuggerPresent() != FALSE;
#else
    return false;
#endif
}

ff_dx12_init_params ff_dx12_init_params_default(void)
{
    ff_dx12_init_params params;
    params.gpu_preference = DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE;
    params.feature_level = D3D_FEATURE_LEVEL_11_0;
    return params;
}

ff_string_view ff_dx12_adapter_name(IDXGIAdapter3* adapter, ff_arena* arena)
{
    ff_string_view result = ff_string_view_empty();
    FF_CHECK_RET_VAL(adapter && arena, result);

    DXGI_ADAPTER_DESC desc = { 0 };
    FF_CHECK_HR_RET_VAL(IDXGIAdapter3_GetDesc(adapter, &desc), result);

    return ff_wide_to_utf8(ff_wz_view(desc.Description), arena, false);
}

static uint64_t get_adapters_hash(IDXGIFactory6* factory)
{
    ff_hash_data hash;
    ff_hash_init(&hash);
    UINT count = 0;

    for (UINT i = 0; ; i++)
    {
        IDXGIAdapter1* adapter = NULL;
        if (FAILED(IDXGIFactory6_EnumAdapters1(factory, i, &adapter)))
        {
            break;
        }

        DXGI_ADAPTER_DESC1 desc = { 0 };
        if (SUCCEEDED(IDXGIAdapter1_GetDesc1(adapter, &desc)))
        {
            ff_hash(&hash, &desc.AdapterLuid, sizeof(desc.AdapterLuid));
            count++;
        }

        IDXGIAdapter1_Release(adapter);
    }

    return count ? ff_hash_done(&hash) : 0;
}

// Fills 'adapters' in preference order and returns how many were found. WARP is appended last
// when the preference-ordered list didn't already include it, so there's always a fallback.
static size_t enum_adapters(IDXGIAdapter3** adapters, size_t max_adapters)
{
    size_t count = 0;

    IDXGIAdapter3* warp_adapter = NULL;
    DXGI_ADAPTER_DESC warp_desc = { 0 };
    bool warp_valid =
        SUCCEEDED(IDXGIFactory6_EnumWarpAdapter(s_factory, &IID_IDXGIAdapter3, (void**)&warp_adapter)) &&
        SUCCEEDED(IDXGIAdapter3_GetDesc(warp_adapter, &warp_desc));
    bool found_warp = false;

    for (UINT i = 0; count < max_adapters; i++)
    {
        IDXGIAdapter3* adapter = NULL;
        DXGI_ADAPTER_DESC desc = { 0 };

        if (FAILED(IDXGIFactory6_EnumAdapterByGpuPreference(s_factory, i, s_gpu_preference, &IID_IDXGIAdapter3, (void**)&adapter)))
        {
            break;
        }

        if (FAILED(IDXGIAdapter3_GetDesc(adapter, &desc)))
        {
            IDXGIAdapter3_Release(adapter);
            break;
        }

        if (warp_valid && desc.VendorId == warp_desc.VendorId && desc.DeviceId == warp_desc.DeviceId)
        {
            found_warp = true;
        }

        ff_arena_declare_stack(name_arena, 256);
        ff_log_write(ff_log_type_debug, FF_SVL("[dx12] Adapter[%u] = %.*s"), (unsigned int)count,
            FF_SV_FORMAT(ff_wide_to_utf8(ff_wz_view(desc.Description), &name_arena, false)));
        ff_arena_destroy(&name_arena);

        adapters[count++] = adapter;
    }

    if (warp_valid && !found_warp && count < max_adapters)
    {
        adapters[count++] = warp_adapter;
    }
    else if (warp_valid)
    {
        IDXGIAdapter3_Release(warp_adapter);
    }

    return count;
}

static ID3D12Device6* create_device(void)
{
    IDXGIAdapter3* adapters[MAX_ADAPTERS];
    size_t adapter_count = enum_adapters(adapters, MAX_ADAPTERS);
    FF_ASSERT_RET_VAL(adapter_count, NULL);

    ID3D12Device6* device = NULL;
    for (size_t i = 0; i < adapter_count; i++)
    {
        if (!device && SUCCEEDED(D3D12CreateDevice((IUnknown*)adapters[i], s_feature_level, &IID_ID3D12Device6, (void**)&device)))
        {
            // Keep looping so the remaining adapters still get released.
        }

        IDXGIAdapter3_Release(adapters[i]);
    }

    if (!device)
    {
        ff_log_write(ff_log_type_debug, FF_SVL("[dx12] D3D12CreateDevice failed"));
        FF_DEBUG_FAIL_RET_VAL(NULL);
    }

    ID3D12Device8* device8 = NULL;
    D3D12_FEATURE_DATA_D3D12_OPTIONS7 options7 = { 0 };
    s_supports_create_heap_not_resident =
        SUCCEEDED(ID3D12Device6_QueryInterface(device, &IID_ID3D12Device8, (void**)&device8)) &&
        SUCCEEDED(ID3D12Device6_CheckFeatureSupport(device, D3D12_FEATURE_D3D12_OPTIONS7, &options7, sizeof(options7)));

    if (device8)
    {
        ID3D12Device8_Release(device8);
    }

    // Bindless (ResourceDescriptorHeap[] in HLSL) needs unbounded descriptor arrays from
    // resource binding tier 3 plus dynamic resource indexing from shader model 6.6.
    D3D12_FEATURE_DATA_D3D12_OPTIONS options = { 0 };
    D3D12_FEATURE_DATA_SHADER_MODEL shader_model = { .HighestShaderModel = D3D_SHADER_MODEL_6_6 };
    s_supports_bindless =
        SUCCEEDED(ID3D12Device6_CheckFeatureSupport(device, D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options))) &&
        options.ResourceBindingTier >= D3D12_RESOURCE_BINDING_TIER_3 &&
        SUCCEEDED(ID3D12Device6_CheckFeatureSupport(device, D3D12_FEATURE_SHADER_MODEL, &shader_model, sizeof(shader_model))) &&
        shader_model.HighestShaderModel >= D3D_SHADER_MODEL_6_6;

    ff_log_write(ff_log_type_debug, FF_SVL("[dx12] D3D12CreateDevice succeeded, node count: %u"),
        (unsigned int)ID3D12Device6_GetNodeCount(device));
    ff_log_write(ff_log_type_debug, FF_SVL("[dx12] - supports non-resident heaps: %d"),
        (int)s_supports_create_heap_not_resident);
    ff_log_write(ff_log_type_debug, FF_SVL("[dx12] - supports bindless: %d"), (int)s_supports_bindless);

    return device;
}

static void enable_debug_layer(void)
{
    ID3D12Debug* debug_interface = NULL;
    FF_CHECK_HR_RET(D3D12GetDebugInterface(&IID_ID3D12Debug, (void**)&debug_interface));

    ID3D12Debug_EnableDebugLayer(debug_interface);
    ID3D12Debug_Release(debug_interface);
}

static void set_debug_info_queue(ID3D12Device6* device)
{
    ID3D12InfoQueue* info_queue = NULL;
    FF_CHECK_HR_RET(ID3D12Device6_QueryInterface(device, &IID_ID3D12InfoQueue, (void**)&info_queue));

    ID3D12InfoQueue_SetBreakOnSeverity(info_queue, D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
    ID3D12InfoQueue_SetBreakOnSeverity(info_queue, D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
    ID3D12InfoQueue_SetBreakOnSeverity(info_queue, D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
    ID3D12InfoQueue_SetBreakOnSeverity(info_queue, D3D12_MESSAGE_SEVERITY_INFO, FALSE);
    ID3D12InfoQueue_SetBreakOnSeverity(info_queue, D3D12_MESSAGE_SEVERITY_MESSAGE, FALSE);

    D3D12_MESSAGE_ID hide[] =
    {
        D3D12_MESSAGE_ID_CREATEPIPELINELIBRARY_INVALIDLIBRARYBLOB,
        D3D12_MESSAGE_ID_CREATEPIPELINELIBRARY_DRIVERVERSIONMISMATCH,
        D3D12_MESSAGE_ID_CREATEPIPELINELIBRARY_ADAPTERVERSIONMISMATCH,
        D3D12_MESSAGE_ID_CREATEPIPELINELIBRARY_UNSUPPORTED,
        D3D12_MESSAGE_ID_CREATEPIPELINESTATE_CACHEDBLOBADAPTERMISMATCH,
        D3D12_MESSAGE_ID_CREATEPIPELINESTATE_CACHEDBLOBDRIVERVERSIONMISMATCH,
        D3D12_MESSAGE_ID_LOADPIPELINE_NAMENOTFOUND,
        D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE, // Windows 11 DXGI bug
        D3D12_MESSAGE_ID_STOREPIPELINE_DUPLICATENAME,
    };

    D3D12_INFO_QUEUE_FILTER filter = { 0 };
    filter.DenyList.NumIDs = (UINT)(sizeof(hide) / sizeof(hide[0]));
    filter.DenyList.pIDList = hide;
    ID3D12InfoQueue_AddStorageFilterEntries(info_queue, &filter);

    ID3D12InfoQueue_Release(info_queue);
}

static DXGI_QUERY_VIDEO_MEMORY_INFO query_video_memory_info(IDXGIAdapter3* adapter)
{
    DXGI_QUERY_VIDEO_MEMORY_INFO total = { 0 };
    FF_ASSERT_RET_VAL(adapter, total);

    const DXGI_MEMORY_SEGMENT_GROUP groups[] = { DXGI_MEMORY_SEGMENT_GROUP_LOCAL, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL };

    for (size_t i = 0; i < sizeof(groups) / sizeof(groups[0]); i++)
    {
        DXGI_QUERY_VIDEO_MEMORY_INFO info = { 0 };
        if (SUCCEEDED(IDXGIAdapter3_QueryVideoMemoryInfo(adapter, 0, groups[i], &info)))
        {
            total.AvailableForReservation += info.AvailableForReservation;
            total.Budget += info.Budget;
            total.CurrentReservation += info.CurrentReservation;
            total.CurrentUsage += info.CurrentUsage;
        }
    }

    return total;
}

void ff_dx12_update_video_memory_info(void)
{
    // No event means budget changes aren't reported, so just refresh every time.
    if (!s_video_memory_change_event || WaitForSingleObject(s_video_memory_change_event, 0) == WAIT_OBJECT_0)
    {
        s_video_memory_info = query_video_memory_info(s_adapter);

        if (s_video_memory_change_event)
        {
            // Manual-reset event, so without this it stays signaled and every later poll
            // reports a budget change that already happened.
            ResetEvent(s_video_memory_change_event);
        }
        ff_log_write(ff_log_type_debug, FF_SVL("[dx12] Video memory budget: %llu bytes, usage: %llu bytes"),
            (unsigned long long)s_video_memory_info.Budget,
            (unsigned long long)s_video_memory_info.CurrentUsage);
    }
}

static bool init_dxgi(bool for_reset)
{
    if (!for_reset)
    {
        DXGIDeclareAdapterRemovalSupport();
    }

    const UINT flags = debug_layer_wanted() ? DXGI_CREATE_FACTORY_DEBUG : 0;
    FF_ASSERT_HR_RET_VAL(CreateDXGIFactory2(flags, &IID_IDXGIFactory6, (void**)&s_factory), false);

    s_adapters_hash = get_adapters_hash(s_factory);
    return true;
}

static void destroy_dxgi(void)
{
    s_adapters_hash = 0;

    if (s_factory)
    {
        IDXGIFactory6_Release(s_factory);
        s_factory = NULL;
    }
}

static bool init_d3d(bool for_reset)
{
    if (!for_reset && debug_layer_wanted())
    {
        enable_debug_layer();
    }

    s_device = create_device();
    FF_ASSERT_RET_VAL(s_device, false);

    if (debug_layer_wanted())
    {
        set_debug_info_queue(s_device);
    }

    // Find the physical adapter that backs the logical device.
    {
        FF_ASSERT(!s_adapter);
        LUID luid = { 0 };
        ID3D12Device6_GetAdapterLuid(s_device, &luid);
        FF_ASSERT_HR_RET_VAL(IDXGIFactory6_EnumAdapterByLuid(s_factory, luid, &IID_IDXGIAdapter3, (void**)&s_adapter), false);

        ff_arena_declare_stack(name_arena, 256);
        ff_log_write(ff_log_type_debug, FF_SVL("[dx12] Final adapter: %.*s"),
            FF_SV_FORMAT(ff_dx12_adapter_name(s_adapter, &name_arena)));
        ff_arena_destroy(&name_arena);
    }

    // Video memory tracking for residency
    {
        // Must query before the event exists. The event is manual-reset and starts unsignaled,
        // so an update with it already created would take the "no change yet" path and leave
        // the budget at zero until the first notification fires.
        ff_dx12_update_video_memory_info();

        s_video_memory_change_event = CreateEventW(NULL, TRUE, FALSE, NULL);

        if (s_video_memory_change_event && FAILED(IDXGIAdapter3_RegisterVideoMemoryBudgetChangeNotificationEvent(
            s_adapter, s_video_memory_change_event, &s_video_memory_change_cookie)))
        {
            CloseHandle(s_video_memory_change_event);
            s_video_memory_change_event = NULL;
            s_video_memory_change_cookie = 0;
        }
    }

    return true;
}

static void keep_alive_node_release(ff_dx12_keep_alive_node* node)
{
    if (node->resource)
    {
        ID3D12Resource_Release(node->resource);
        node->resource = NULL;
    }

    ff_dx12_mem_range_free(&node->mem_range);
    ff_dx12_fence_values_clear(&node->fence_values);
}

void ff_dx12_flush_keep_alive(void)
{
    // Entries are pushed in roughly fence order, so stopping at the first incomplete one keeps
    // this O(1) amortized instead of scanning the whole list every frame.
    while (s_keep_alive_head && ff_dx12_fence_values_complete(&s_keep_alive_head->fence_values))
    {
        ff_dx12_keep_alive_node* node = s_keep_alive_head;
        s_keep_alive_head = node->next;

        if (!s_keep_alive_head)
        {
            s_keep_alive_tail = NULL;
        }

        keep_alive_node_release(node);

        node->next = s_keep_alive_free;
        s_keep_alive_free = node;
    }
}

void ff_dx12_keep_alive_resource(ID3D12Resource* resource, const ff_dx12_mem_range* mem_range,
    const ff_dx12_fence_values* fence_values)
{
    if (!s_keep_alive_arena_valid)
    {
        ff_arena_init_heap_local(&s_keep_alive_arena, 4096);
        s_keep_alive_arena_valid = true;
    }

    ff_dx12_keep_alive_node pending = { 0 };
    pending.resource = resource;

    if (mem_range)
    {
        pending.mem_range = *mem_range;
    }

    // Deep copy: the caller's set may have spilled into an arena that dies with the caller, and
    // this node outlives it. Re-adding rebuilds the spill against the keep-alive arena.
    ff_dx12_fence_values_init_arena(&pending.fence_values, &s_keep_alive_arena);

    if (fence_values)
    {
        ff_dx12_fence_values_add_all(&pending.fence_values, fence_values);
    }

    // Nothing in flight, so skip the list entirely and release right now.
    if (ff_dx12_fence_values_complete(&pending.fence_values))
    {
        keep_alive_node_release(&pending);
        return;
    }

    ff_dx12_keep_alive_node* node = s_keep_alive_free;
    if (node)
    {
        s_keep_alive_free = node->next;
    }
    else
    {
        node = ff_arena_alloc_type(&s_keep_alive_arena, ff_dx12_keep_alive_node, 1);
    }

    if (!node)
    {
        // Can't defer, so fall back to blocking rather than leaking the resource. Only safe for
        // fence values that were actually submitted; an unsubmitted one would hang forever, so
        // leak it instead. Reaching here at all means arena allocation failed.
        if (ff_dx12_fence_values_wait_is_pending(&pending.fence_values))
        {
            ff_dx12_fence_values_wait(&pending.fence_values, NULL);
            keep_alive_node_release(&pending);
        }

        FF_DEBUG_FAIL_RET();
    }

    *node = pending;
    node->next = NULL;

    if (s_keep_alive_tail)
    {
        s_keep_alive_tail->next = node;
    }
    else
    {
        s_keep_alive_head = node;
    }

    s_keep_alive_tail = node;
}

static void keep_alive_destroy(void)
{
    // Runs after the queues, and therefore their fences, are already destroyed, so the fence
    // values in these nodes must not be consulted at all. The GPU is idle by this point
    // (wait_for_idle ran first), so everything still queued is safe to release outright.
    for (ff_dx12_keep_alive_node* node = s_keep_alive_head; node; node = node->next)
    {
        keep_alive_node_release(node);
    }

    s_keep_alive_head = NULL;
    s_keep_alive_tail = NULL;
    s_keep_alive_free = NULL;

    if (s_keep_alive_arena_valid)
    {
        ff_arena_destroy(&s_keep_alive_arena);
        s_keep_alive_arena_valid = false;
    }
}

ff_dx12_queue* ff_dx12_direct_queue(void)
{
    if (!ff_dx12_queue_valid(&s_direct_queue))
    {
        FF_ASSERT_RET_VAL(ff_dx12_queue_init(&s_direct_queue, FF_SVL("Direct queue"), D3D12_COMMAND_LIST_TYPE_DIRECT), NULL);
    }

    return &s_direct_queue;
}

ff_dx12_queue* ff_dx12_copy_queue(void)
{
    if (!ff_dx12_queue_valid(&s_copy_queue))
    {
        FF_ASSERT_RET_VAL(ff_dx12_queue_init(&s_copy_queue, FF_SVL("Copy queue"), D3D12_COMMAND_LIST_TYPE_COPY), NULL);
    }

    return &s_copy_queue;
}

ff_dx12_queue* ff_dx12_compute_queue(void)
{
    if (!ff_dx12_queue_valid(&s_compute_queue))
    {
        FF_ASSERT_RET_VAL(ff_dx12_queue_init(&s_compute_queue, FF_SVL("Compute queue"), D3D12_COMMAND_LIST_TYPE_COMPUTE), NULL);
    }

    return &s_compute_queue;
}

ff_dx12_queue* ff_dx12_queue_from_type(D3D12_COMMAND_LIST_TYPE type)
{
    switch (type)
    {
        case D3D12_COMMAND_LIST_TYPE_COPY:
            return ff_dx12_copy_queue();

        case D3D12_COMMAND_LIST_TYPE_COMPUTE:
            return ff_dx12_compute_queue();

        default:
            return ff_dx12_direct_queue();
    }
}

static ff_dx12_mem_allocator* get_mem_allocator(ff_dx12_mem_allocator_index index)
{
    if (!s_mem_allocator_valid[index])
    {
        // Sizes match the old C++ globals. The ring allocators ignore max_size.
        static const struct
        {
            uint64_t initial_size;
            uint64_t max_size;
            ff_dx12_heap_usage usage;
            bool ring;
        } s_params[ff_dx12_mem_allocator_count] =
        {
            [ff_dx12_mem_allocator_upload] = { .initial_size = FF_DX12_ONE_MEG, .usage = ff_dx12_heap_usage_upload, .ring = true },
            [ff_dx12_mem_allocator_readback] = { .initial_size = FF_DX12_ONE_MEG, .usage = ff_dx12_heap_usage_readback, .ring = true },
            [ff_dx12_mem_allocator_dynamic_buffer] = { .initial_size = FF_DX12_ONE_MEG, .usage = ff_dx12_heap_usage_gpu_buffers, .ring = true },
            [ff_dx12_mem_allocator_static_buffer] = { .initial_size = FF_DX12_ONE_MEG, .max_size = FF_DX12_ONE_MEG * 32, .usage = ff_dx12_heap_usage_gpu_buffers },
            [ff_dx12_mem_allocator_texture] = { .initial_size = FF_DX12_ONE_MEG * 4, .max_size = FF_DX12_ONE_MEG * 64, .usage = ff_dx12_heap_usage_gpu_textures },
            [ff_dx12_mem_allocator_target] = { .initial_size = FF_DX12_ONE_MEG * 16, .max_size = FF_DX12_ONE_MEG * 128, .usage = ff_dx12_heap_usage_gpu_targets },
        };

        ff_dx12_mem_allocator_init(&s_mem_allocators[index], s_params[index].initial_size,
            s_params[index].max_size, s_params[index].usage, s_params[index].ring);
        s_mem_allocator_valid[index] = true;
    }

    return &s_mem_allocators[index];
}

ff_dx12_mem_allocator* ff_dx12_upload_allocator(void)
{
    return get_mem_allocator(ff_dx12_mem_allocator_upload);
}

ff_dx12_mem_allocator* ff_dx12_readback_allocator(void)
{
    return get_mem_allocator(ff_dx12_mem_allocator_readback);
}

ff_dx12_mem_allocator* ff_dx12_dynamic_buffer_allocator(void)
{
    return get_mem_allocator(ff_dx12_mem_allocator_dynamic_buffer);
}

ff_dx12_mem_allocator* ff_dx12_static_buffer_allocator(void)
{
    return get_mem_allocator(ff_dx12_mem_allocator_static_buffer);
}

ff_dx12_mem_allocator* ff_dx12_texture_allocator(void)
{
    return get_mem_allocator(ff_dx12_mem_allocator_texture);
}

ff_dx12_mem_allocator* ff_dx12_target_allocator(void)
{
    return get_mem_allocator(ff_dx12_mem_allocator_target);
}

static ff_dx12_cpu_descriptor_allocator* get_cpu_descriptors(D3D12_DESCRIPTOR_HEAP_TYPE type, size_t bucket_size)
{
    if (!s_cpu_descriptor_allocator_valid[type])
    {
        ff_dx12_cpu_descriptor_allocator_init(&s_cpu_descriptor_allocators[type], type, bucket_size);
        s_cpu_descriptor_allocator_valid[type] = true;
    }

    return &s_cpu_descriptor_allocators[type];
}

ff_dx12_cpu_descriptor_allocator* ff_dx12_cpu_buffer_descriptors(void)
{
    return get_cpu_descriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 256);
}

ff_dx12_cpu_descriptor_allocator* ff_dx12_cpu_sampler_descriptors(void)
{
    return get_cpu_descriptors(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 32);
}

ff_dx12_cpu_descriptor_allocator* ff_dx12_cpu_target_descriptors(void)
{
    return get_cpu_descriptors(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 32);
}

ff_dx12_cpu_descriptor_allocator* ff_dx12_cpu_depth_descriptors(void)
{
    return get_cpu_descriptors(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 32);
}

static ff_dx12_gpu_descriptor_allocator* get_gpu_descriptors(D3D12_DESCRIPTOR_HEAP_TYPE type, size_t pinned_size, size_t ring_size)
{
    if (!s_gpu_descriptor_allocator_valid[type])
    {
        FF_ASSERT_RET_VAL(ff_dx12_gpu_descriptor_allocator_init(
            &s_gpu_descriptor_allocators[type], type, pinned_size, ring_size), NULL);
        s_gpu_descriptor_allocator_valid[type] = true;
    }

    return &s_gpu_descriptor_allocators[type];
}

ff_dx12_gpu_descriptor_allocator* ff_dx12_gpu_view_descriptors(void)
{
    // Hardware max is 1,000,000, so there is plenty of room to grow this later.
    return get_gpu_descriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 256, 7936);
}

ff_dx12_gpu_descriptor_allocator* ff_dx12_gpu_sampler_descriptors(void)
{
    // Hardware max is 2048.
    return get_gpu_descriptors(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 128, 1920);
}

static void destroy_allocators(void)
{
    for (size_t i = 0; i < ff_dx12_mem_allocator_count; i++)
    {
        if (s_mem_allocator_valid[i])
        {
            ff_dx12_mem_allocator_destroy(&s_mem_allocators[i]);
            s_mem_allocator_valid[i] = false;
        }
    }

    for (size_t i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++)
    {
        if (s_cpu_descriptor_allocator_valid[i])
        {
            ff_dx12_cpu_descriptor_allocator_destroy(&s_cpu_descriptor_allocators[i]);
            s_cpu_descriptor_allocator_valid[i] = false;
        }

        if (s_gpu_descriptor_allocator_valid[i])
        {
            ff_dx12_gpu_descriptor_allocator_destroy(&s_gpu_descriptor_allocators[i]);
            s_gpu_descriptor_allocator_valid[i] = false;
        }
    }
}

void ff_dx12_forget_residency_data(ff_dx12_residency_data* data)
{
    FF_CHECK_RET(data);

    if (ff_dx12_queue_valid(&s_copy_queue))
    {
        ff_dx12_queue_forget_residency_data(&s_copy_queue, data);
    }

    if (ff_dx12_queue_valid(&s_compute_queue))
    {
        ff_dx12_queue_forget_residency_data(&s_compute_queue, data);
    }

    if (ff_dx12_queue_valid(&s_direct_queue))
    {
        ff_dx12_queue_forget_residency_data(&s_direct_queue, data);
    }
}

void ff_dx12_wait_for_idle(void)
{
    if (ff_dx12_queue_valid(&s_copy_queue))
    {
        ff_dx12_queue_wait_for_idle(&s_copy_queue);
    }

    if (ff_dx12_queue_valid(&s_compute_queue))
    {
        ff_dx12_queue_wait_for_idle(&s_compute_queue);
    }

    if (ff_dx12_queue_valid(&s_direct_queue))
    {
        ff_dx12_queue_wait_for_idle(&s_direct_queue);
    }

    ff_dx12_flush_keep_alive();
}

void ff_dx12_frame_started(void)
{
    ff_dx12_flush_keep_alive();
    ff_dx12_update_video_memory_info();
}

void ff_dx12_frame_complete(void)
{
    s_frame_count++;

    // The old code drove this through a frame_complete signal that each allocator subscribed to.
    // Here globals owns the allocators, so it retires their in-flight ranges directly.
    for (size_t i = 0; i < ff_dx12_mem_allocator_count; i++)
    {
        if (s_mem_allocator_valid[i])
        {
            ff_dx12_mem_allocator_frame_complete(&s_mem_allocators[i]);
        }
    }
}

uint64_t ff_dx12_frame_count(void)
{
    return s_frame_count;
}

size_t ff_dx12_fix_sample_count(DXGI_FORMAT format, size_t sample_count)
{
    size_t fixed_sample_count = sample_count ? ff_math_round_up_pow2(sample_count) : 1;

    while (fixed_sample_count > 1)
    {
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS levels = { 0 };
        levels.Format = format;
        levels.SampleCount = (UINT)fixed_sample_count;

        if (SUCCEEDED(ID3D12Device6_CheckFeatureSupport(ff_dx12_device(),
            D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &levels, sizeof(levels))) && levels.NumQualityLevels)
        {
            break;
        }

        fixed_sample_count /= 2;
    }

    return ff_math_max_size(fixed_sample_count, 1);
}

static void destroy_d3d(bool for_reset)
{
    // Queues must go before residency so that wait_for_idle can still retire GPU work, and the
    // keep-alive list must be drained after that so nothing outlives its residency_data.
    //
    // The wait is skipped only when the device is already lost: its queues can never make progress
    // again, so waiting would hang forever, and the driver has already retired the work. When the
    // device is still healthy (a forced or adapter-change reset) the GPU really is still running,
    // and releasing resources without draining it first would pull them out from under it.
    if (ff_dx12_device_valid() && (ff_dx12_queue_valid(&s_direct_queue) || ff_dx12_queue_valid(&s_copy_queue) || ff_dx12_queue_valid(&s_compute_queue)))
    {
        ff_dx12_wait_for_idle();
    }

    ff_dx12_queue_destroy(&s_direct_queue);
    ff_dx12_queue_destroy(&s_copy_queue);
    ff_dx12_queue_destroy(&s_compute_queue);

    keep_alive_destroy();
    s_frame_count = 0;

    // On a reset the allocators are deliberately kept: their buffers and buckets hold the
    // offsets and indices that every outstanding mem_range and descriptor_range points at, so
    // destroying them would dangle every one of those. dx12_reset.c has already released just the
    // GPU objects inside them through their before_reset hooks.
    if (!for_reset)
    {
        // Allocators own heaps and descriptor heaps, and every heap registers residency data, so
        // they have to go after the keep-alive drain but before residency shuts down.
        destroy_allocators();
    }

    ff_dx12_residency_destroy();

    if (s_video_memory_change_event)
    {
        IDXGIAdapter3_UnregisterVideoMemoryBudgetChangeNotification(s_adapter, s_video_memory_change_cookie);
        CloseHandle(s_video_memory_change_event);
        s_video_memory_change_event = NULL;
        s_video_memory_change_cookie = 0;
    }

    s_video_memory_info = (DXGI_QUERY_VIDEO_MEMORY_INFO){ 0 };
    s_supports_create_heap_not_resident = false;
    s_supports_bindless = false;
    s_simulate_device_invalid = false;

    if (s_adapter)
    {
        IDXGIAdapter3_Release(s_adapter);
        s_adapter = NULL;
    }

    if (s_device)
    {
        ID3D12Device6_Release(s_device);
        s_device = NULL;
    }
}

bool ff_dx12_init(const ff_dx12_init_params* params)
{
    ff_dx12_init_params defaults = ff_dx12_init_params_default();
    if (!params)
    {
        params = &defaults;
    }

    FF_ASSERT_RET_VAL(!s_factory && !s_device, false);

    s_gpu_preference = params->gpu_preference;
    s_feature_level = params->feature_level;

    if (!init_dxgi(false) || !init_d3d(false) || !ff_dx12_residency_init())
    {
        ff_dx12_destroy();
        return false;
    }

    return true;
}

bool internal_ff_dx12_init_dxgi(bool for_reset)
{
    return init_dxgi(for_reset);
}

void internal_ff_dx12_destroy_dxgi(void)
{
    destroy_dxgi();
}

bool internal_ff_dx12_init_d3d(bool for_reset)
{
    return init_d3d(for_reset);
}

void internal_ff_dx12_destroy_d3d(bool for_reset)
{
    destroy_d3d(for_reset);
}

void internal_ff_dx12_clear_fatal_error(void)
{
    s_simulate_device_invalid = false;
}

void internal_ff_dx12_allocators_before_reset(void)
{
    // Only allocators that were actually created are touched; the lazy accessors must not be used
    // here, since creating an allocator against a dying device would immediately fail.
    for (size_t i = 0; i < ff_dx12_mem_allocator_count; i++)
    {
        if (s_mem_allocator_valid[i])
        {
            internal_ff_dx12_mem_allocator_before_reset(&s_mem_allocators[i]);
        }
    }

    for (size_t i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++)
    {
        if (s_cpu_descriptor_allocator_valid[i])
        {
            internal_ff_dx12_cpu_descriptor_allocator_before_reset(&s_cpu_descriptor_allocators[i]);
        }

        if (s_gpu_descriptor_allocator_valid[i])
        {
            internal_ff_dx12_gpu_descriptor_allocator_before_reset(&s_gpu_descriptor_allocators[i]);
        }
    }
}

bool internal_ff_dx12_allocators_reset(void)
{
    bool result = true;

    for (size_t i = 0; i < ff_dx12_mem_allocator_count; i++)
    {
        if (s_mem_allocator_valid[i])
        {
            result = internal_ff_dx12_mem_allocator_reset(&s_mem_allocators[i]) && result;
        }
    }

    for (size_t i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++)
    {
        if (s_cpu_descriptor_allocator_valid[i])
        {
            result = internal_ff_dx12_cpu_descriptor_allocator_reset(&s_cpu_descriptor_allocators[i]) && result;
        }

        if (s_gpu_descriptor_allocator_valid[i])
        {
            result = internal_ff_dx12_gpu_descriptor_allocator_reset(&s_gpu_descriptor_allocators[i]) && result;
        }
    }

    return result;
}

void ff_dx12_destroy(void)
{
    internal_ff_dx12_reset_shutdown();
    destroy_d3d(false);
    destroy_dxgi();

    s_gpu_preference = (DXGI_GPU_PREFERENCE)0;
    s_feature_level = (D3D_FEATURE_LEVEL)0;
}

IDXGIFactory6* ff_dx12_factory(void)
{
    return s_factory;
}

IDXGIAdapter3* ff_dx12_adapter(void)
{
    return s_adapter;
}

ID3D12Device6* ff_dx12_device(void)
{
    return s_device;
}

D3D_FEATURE_LEVEL ff_dx12_feature_level(void)
{
    return s_feature_level;
}

bool ff_dx12_device_valid(void)
{
    return !s_simulate_device_invalid && s_device && ID3D12Device6_GetDeviceRemovedReason(s_device) == S_OK;
}

void ff_dx12_device_fatal_error(ff_string_view reason)
{
    ff_log_write(ff_log_type_debug, FF_SVL("[dx12] Removing device after fatal error: %.*s"), FF_SV_FORMAT(reason));

    if (ff_dx12_device_valid())
    {
        s_simulate_device_invalid = true;
    }
}

bool ff_dx12_supports_create_heap_not_resident(void)
{
    return s_supports_create_heap_not_resident;
}

bool ff_dx12_supports_bindless(void)
{
    return s_supports_bindless;
}

bool ff_dx12_factory_current(void)
{
    return s_factory && IDXGIFactory6_IsCurrent(s_factory) != FALSE;
}

uint64_t ff_dx12_adapters_hash(void)
{
    return s_adapters_hash;
}

DXGI_QUERY_VIDEO_MEMORY_INFO ff_dx12_video_memory_info(void)
{
    return s_video_memory_info;
}
