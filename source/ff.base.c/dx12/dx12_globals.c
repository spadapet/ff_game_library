#include "pch.h"
#include "base/arena.h"
#include "base/assert.h"
#include "base/hash.h"
#include "base/log.h"
#include "base/string.h"
#include "dx12/dx12_globals.h"

// Embedding the link dependencies here keeps them with the code that needs them, and they
// automatically flow to anything that links this static library.
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

// Max adapters enumerated in one pass. Way more than any real machine has, and it keeps
// adapter handling on the stack instead of needing a dynamic array.
#define MAX_ADAPTERS 16

static IDXGIFactory6* s_factory;
static IDXGIAdapter3* s_adapter;
static ID3D12Device6* s_device;
static DXGI_GPU_PREFERENCE s_gpu_preference;
static D3D_FEATURE_LEVEL s_feature_level;
static uint64_t s_adapters_hash;
static bool s_supports_create_heap_not_resident;
static bool s_simulate_device_invalid;

static HANDLE s_video_memory_change_event;
static DWORD s_video_memory_change_cookie;
static DXGI_QUERY_VIDEO_MEMORY_INFO s_video_memory_info;

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

    ff_log_write(ff_log_type_debug, FF_SVL("[dx12] D3D12CreateDevice succeeded, node count: %u"),
        (unsigned int)ID3D12Device6_GetNodeCount(device));
    ff_log_write(ff_log_type_debug, FF_SVL("[dx12] - supports non-resident heaps: %d"),
        (int)s_supports_create_heap_not_resident);

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
        s_video_memory_change_event = CreateEventW(NULL, TRUE, FALSE, NULL);

        if (s_video_memory_change_event && FAILED(IDXGIAdapter3_RegisterVideoMemoryBudgetChangeNotificationEvent(
            s_adapter, s_video_memory_change_event, &s_video_memory_change_cookie)))
        {
            CloseHandle(s_video_memory_change_event);
            s_video_memory_change_event = NULL;
            s_video_memory_change_cookie = 0;
        }

        ff_dx12_update_video_memory_info();
    }

    return true;
}

static void destroy_d3d(void)
{
    if (s_video_memory_change_event)
    {
        IDXGIAdapter3_UnregisterVideoMemoryBudgetChangeNotification(s_adapter, s_video_memory_change_cookie);
        CloseHandle(s_video_memory_change_event);
        s_video_memory_change_event = NULL;
        s_video_memory_change_cookie = 0;
    }

    s_video_memory_info = (DXGI_QUERY_VIDEO_MEMORY_INFO){ 0 };
    s_supports_create_heap_not_resident = false;
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

    if (!init_dxgi(false) || !init_d3d(false))
    {
        ff_dx12_destroy();
        return false;
    }

    return true;
}

void ff_dx12_destroy(void)
{
    destroy_d3d();
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
