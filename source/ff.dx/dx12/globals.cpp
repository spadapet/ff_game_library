#include "pch.h"
#include "dx12/descriptor_allocator.h"
#include "dx12/draw_device.h"
#include "dx12/fence.h"
#include "dx12/globals.h"
#include "dx12/gpu_event.h"
#include "dx12/mem_allocator.h"
#include "dx12/object_cache.h"
#include "dx12/queue.h"
#include "dx12/queues.h"
#include "dx12/resource.h"
#include "ff.dx12.res.h"

extern "C"
{
    // NVIDIA and AMD globals to prefer their higher powered GPU over Intel
    __declspec(dllexport) extern const DWORD NvOptimusEnablement = 1;
    __declspec(dllexport) extern const int AmdPowerXpressRequestHighPerformance = 1;
    __declspec(dllexport) extern const UINT D3D12SDKVersion = D3D12_AGILITY_SDK_VERSION_EXPORT;
    __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\";
}

// DX12 globals
static Microsoft::WRL::ComPtr<ID3D12Device1> device;
static Microsoft::WRL::ComPtr<IDXGIAdapter3> adapter;
static Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
static const ff::dxgi::host_functions* host_functions;
static D3D_FEATURE_LEVEL feature_level;
static size_t adapters_hash;
static size_t outputs_hash;
static bool supports_create_heap_not_resident_;
static bool supports_mesh_shaders_;
static bool simulate_device_invalid;

// Device children
static std::mutex device_children_mutex;
static ff::dxgi::device_child_base* first_device_child;
static ff::dxgi::device_child_base* last_device_child;
static ff::signal<ff::dxgi::device_child_base*> removed_device_child_signal;

// Frame data
static std::unique_ptr<ff::dx12::commands> frame_commands;
static ff::signal<size_t> frame_complete_signal;
static size_t frame_count;

// GPU memory residency
static std::unique_ptr<ff::win_event> video_memory_change_event;
static DWORD video_memory_change_event_cookie{};
static DXGI_QUERY_VIDEO_MEMORY_INFO video_memory_info{};
static std::unique_ptr<ff::dx12::fence> residency_fence;

// Resource keep alive until unused by GPU
static std::mutex keep_alive_mutex;
static std::list<std::pair<ff::dx12::resource, ff::dx12::fence_values>> keep_alive_resources;

static std::unique_ptr<ff::resource_objects> shader_resources;
static std::unique_ptr<ff::dxgi::draw_device_base> draw_device;
static std::unique_ptr<ff::dx12::object_cache> object_cache;
static std::unique_ptr<ff::dx12::queues> queues;
static std::array<std::unique_ptr<ff::dx12::cpu_descriptor_allocator>, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> cpu_descriptor_allocators;
static std::array<std::unique_ptr<ff::dx12::gpu_descriptor_allocator>, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> gpu_descriptor_allocators;
static std::unique_ptr<ff::dx12::mem_allocator_ring> upload_allocator;
static std::unique_ptr<ff::dx12::mem_allocator_ring> readback_allocator;
static std::unique_ptr<ff::dx12::mem_allocator_ring> dynamic_buffer_allocator;
static std::unique_ptr<ff::dx12::mem_allocator> static_buffer_allocator;
static std::unique_ptr<ff::dx12::mem_allocator> texture_allocator;
static std::unique_ptr<ff::dx12::mem_allocator> target_allocator;

static std::string adapter_name(IDXGIAdapter* adapter)
{
    DXGI_ADAPTER_DESC desc{};
    if (SUCCEEDED(adapter->GetDesc(&desc)))
    {
        return ff::string::to_string(std::wstring_view(&desc.Description[0]));
    }

    debug_fail_ret_val("");
}

static Microsoft::WRL::ComPtr<ID3D12Device1> create_dx12_device()
{
    std::vector<Microsoft::WRL::ComPtr<IDXGIAdapter>> adapters;
    for (UINT i = 0; adapters.size() == i; i++)
    {
        Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
        if (SUCCEEDED(::factory->EnumAdapters(i, adapter.GetAddressOf())))
        {
            ff::log::write(ff::log::type::dx12, "Adapter[", adapters.size(), "] = ", ::adapter_name(adapter.Get()));
            adapters.push_back(std::move(adapter));
        }
    }

    for (size_t i = 0; i < adapters.size(); i++)
    {
        Microsoft::WRL::ComPtr<ID3D12Device1> device;

        if (SUCCEEDED(::D3D12CreateDevice(adapters[i].Get(), ::feature_level, IID_PPV_ARGS(&device))))
        {
            ff::log::write(ff::log::type::dx12, "D3D12CreateDevice succeeded, adapter index=", i, ", node count=", device->GetNodeCount());
            return device;
        }
    }

    ff::log::write_debug_fail(ff::log::type::dx12, "D3D12CreateDevice failed");
    return nullptr;
}

static DXGI_QUERY_VIDEO_MEMORY_INFO get_video_memory_info(IDXGIAdapter* adapter)
{
    DXGI_QUERY_VIDEO_MEMORY_INFO info{};
    Microsoft::WRL::ComPtr<IDXGIAdapter3> adapter3;

    if (adapter && SUCCEEDED(adapter->QueryInterface(IID_PPV_ARGS(&adapter3))))
    {
        DXGI_QUERY_VIDEO_MEMORY_INFO info1;
        if (SUCCEEDED(adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info1)))
        {
            info.AvailableForReservation += info1.AvailableForReservation;
            info.Budget += info1.Budget;
            info.CurrentReservation += info1.CurrentReservation;
            info.CurrentUsage += info1.CurrentUsage;
        }

        if (SUCCEEDED(adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &info1)))
        {
            info.AvailableForReservation += info1.AvailableForReservation;
            info.Budget += info1.Budget;
            info.CurrentReservation += info1.CurrentReservation;
            info.CurrentUsage += info1.CurrentUsage;
        }
    }

    return info;
}

static void update_video_memory_info()
{
    if (!::video_memory_change_event || ::video_memory_change_event->is_set())
    {
        ::video_memory_info = ::get_video_memory_info(ff::dx12::adapter());

        if (::video_memory_change_event)
        {
            ::video_memory_change_event->reset();
        }

        ff::log::write(ff::log::type::dx12_residency, "Video memory budget:", ::video_memory_info.Budget, " bytes, Usage:", ::video_memory_info.CurrentUsage, " bytes");
    }
}

static void flush_keep_alive()
{
    std::scoped_lock lock(::keep_alive_mutex);

    while (!::keep_alive_resources.empty())
    {
        auto& [resource, fence_values] = ::keep_alive_resources.front();
        if (fence_values.complete())
        {
            ::keep_alive_resources.pop_front();
        }
        else
        {
            break;
        }
    }
}

static bool is_graphics_debugger_present()
{
    return ::GetModuleHandle(L"DXCaptureReplay.dll") != nullptr; // ::GetModuleHandle(L"renderdoc.dll");
}

static bool supports_create_heap_not_resident()
{
    Microsoft::WRL::ComPtr<ID3D12Device8> device8;
    D3D12_FEATURE_DATA_D3D12_OPTIONS7 options7{};
    if (SUCCEEDED(::device.As(&device8)) || SUCCEEDED(::device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &options7, sizeof(options7))))
    {
        return !ff::constants::profile_build || !::is_graphics_debugger_present();
    }

    return false;
}

static bool supports_mesh_shaders()
{
    D3D12_FEATURE_DATA_D3D12_OPTIONS7 options7{};
    return SUCCEEDED(::device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &options7, sizeof(options7))) &&
        options7.MeshShaderTier >= D3D12_MESH_SHADER_TIER_1;
}

static size_t get_adapters_hash(IDXGIFactory* factory)
{
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    ff::stable_hash_data_t hash;
    UINT i = 0;

    for (; SUCCEEDED(factory->EnumAdapters(i, &adapter)); i++, adapter.Reset())
    {
        DXGI_ADAPTER_DESC desc;
        if (SUCCEEDED(adapter->GetDesc(&desc)))
        {
            hash.hash(&desc.AdapterLuid, sizeof(desc.AdapterLuid));
        }
    }

    return i ? hash.hash() : 0;
}

static Microsoft::WRL::ComPtr<IDXGIAdapter> fix_adapter(IDXGIFactory* dxgi, Microsoft::WRL::ComPtr<IDXGIAdapter> adapter)
{
    DXGI_ADAPTER_DESC desc;
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter2;
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory4;

    return SUCCEEDED(dxgi->QueryInterface(IID_PPV_ARGS(&factory4))) &&
        SUCCEEDED(adapter->GetDesc(&desc)) &&
        SUCCEEDED(factory4->EnumAdapterByLuid(desc.AdapterLuid, IID_PPV_ARGS(&adapter2))) ? adapter2 : adapter;
}

static size_t get_outputs_hash(IDXGIFactory* factory, IDXGIAdapter* adapter)
{
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter2 = ::fix_adapter(factory, adapter);
    Microsoft::WRL::ComPtr<IDXGIOutput> output;
    ff::stable_hash_data_t hash;
    UINT i = 0;

    for (; SUCCEEDED(adapter2->EnumOutputs(i++, &output)); output.Reset())
    {
        DXGI_OUTPUT_DESC desc;
        if (SUCCEEDED(output->GetDesc(&desc)))
        {
            ff::log::write(ff::log::type::dxgi, "Adapter Output[", i - 1, "] = ", ff::string::to_string(std::wstring_view(&desc.DeviceName[0])));
            hash.hash(&desc.Monitor, sizeof(desc.Monitor));
        }
    }

    return i ? hash.hash() : 0;
}

static Microsoft::WRL::ComPtr<IDXGIFactory4> create_factory()
{
    const UINT flags = (ff::constants::debug_build && ::IsDebuggerPresent()) ? DXGI_CREATE_FACTORY_DEBUG : 0;

    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    return SUCCEEDED(::CreateDXGIFactory2(flags, IID_PPV_ARGS(&factory))) ? factory : nullptr;
}

static bool init_dxgi()
{
    ::DXGIDeclareAdapterRemovalSupport();

    ::factory = ::create_factory();
    assert_ret_val(::factory, false);
    ::adapters_hash = ::get_adapters_hash(::factory.Get());

    return true;
}

static void destroy_dxgi()
{
    ::adapters_hash = 0;
    ::factory.Reset();
}

static bool init_d3d(bool for_reset)
{
    if (!for_reset && ff::constants::debug_build && ::IsDebuggerPresent())
    {
        Microsoft::WRL::ComPtr<ID3D12Debug> debug_interface;
        if (SUCCEEDED(::D3D12GetDebugInterface(IID_PPV_ARGS(&debug_interface))))
        {
            debug_interface->EnableDebugLayer();
        }

        // Could also check for ID3D12DeviceRemovedExtendedDataSettings
    }

    // Create the logical device
    {
        assert(!::device);
        ::device = ::create_dx12_device();
        assert_ret_val(::device, false);
    }

    ::supports_create_heap_not_resident_ = ::supports_create_heap_not_resident();
    ::supports_mesh_shaders_ = ::supports_mesh_shaders();

    // Break on debug error
    if (ff::constants::debug_build && ::IsDebuggerPresent())
    {
        Microsoft::WRL::ComPtr<ID3D12InfoQueue> info_queue;

        if (SUCCEEDED(::device.As(&info_queue)))
        {
            info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
            info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
            info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
            info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_INFO, FALSE);
            info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_MESSAGE, FALSE);

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
            
            D3D12_INFO_QUEUE_FILTER filter{};
            filter.DenyList.NumIDs = _countof(hide);
            filter.DenyList.pIDList = hide;
            info_queue->AddStorageFilterEntries(&filter);
        }
    }

    // Find the physical adapter for the logical device
    {
        assert(!::adapter);
        LUID luid = ::device->GetAdapterLuid();
        assert_hr_ret_val(::factory->EnumAdapterByLuid(luid, IID_PPV_ARGS(&::adapter)), false);
        ::outputs_hash = ::get_outputs_hash(::factory.Get(), ::adapter.Get());

        ff::log::write(ff::log::type::dx12, "Final adapter: ", ::adapter_name(::adapter.Get()));
    }

    // Video memory for residency
    {
        ::update_video_memory_info();
        ::video_memory_change_event = std::make_unique<ff::win_event>();
        if (FAILED(::adapter->RegisterVideoMemoryBudgetChangeNotificationEvent(*::video_memory_change_event, &::video_memory_change_event_cookie)))
        {
            ::video_memory_change_event.reset();
            ::video_memory_change_event_cookie = 0;
        }
    }

    // Shader resources
    if (!for_reset)
    {
        ff::data_reader assets_reader(::assets::dx12::data());
        ::shader_resources = std::make_unique<ff::resource_objects>(assets_reader);
    }

    if (!for_reset)
    {
        const uint64_t one_meg = static_cast<uint64_t>(1024) * static_cast<uint64_t>(1024);

        ::upload_allocator = std::make_unique<ff::dx12::mem_allocator_ring>(one_meg, ff::dx12::heap::usage_t::upload);
        ::readback_allocator = std::make_unique<ff::dx12::mem_allocator_ring>(one_meg, ff::dx12::heap::usage_t::readback);
        ::dynamic_buffer_allocator = std::make_unique<ff::dx12::mem_allocator_ring>(one_meg, ff::dx12::heap::usage_t::gpu_buffers);
        ::static_buffer_allocator = std::make_unique<ff::dx12::mem_allocator>(one_meg, one_meg * 8, ff::dx12::heap::usage_t::gpu_buffers);
        ::texture_allocator = std::make_unique<ff::dx12::mem_allocator>(one_meg * 4, one_meg * 32, ff::dx12::heap::usage_t::gpu_textures);
        ::target_allocator = std::make_unique<ff::dx12::mem_allocator>(one_meg * 16, one_meg * 128, ff::dx12::heap::usage_t::gpu_targets);
        ::cpu_descriptor_allocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] = std::make_unique<ff::dx12::cpu_descriptor_allocator>(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 256);
        ::cpu_descriptor_allocators[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER] = std::make_unique<ff::dx12::cpu_descriptor_allocator>(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 32);
        ::cpu_descriptor_allocators[D3D12_DESCRIPTOR_HEAP_TYPE_RTV] = std::make_unique<ff::dx12::cpu_descriptor_allocator>(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 32);
        ::cpu_descriptor_allocators[D3D12_DESCRIPTOR_HEAP_TYPE_DSV] = std::make_unique<ff::dx12::cpu_descriptor_allocator>(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 32);
        ::gpu_descriptor_allocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] = std::make_unique<ff::dx12::gpu_descriptor_allocator>(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 256, 7936); // max is 1,000,000 so quite a way to go
        ::gpu_descriptor_allocators[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER] = std::make_unique<ff::dx12::gpu_descriptor_allocator>(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 128, 1920); // max is 2048
        ::queues = std::make_unique<ff::dx12::queues>();
        ::object_cache = std::make_unique<ff::dx12::object_cache>();
        ::draw_device = ff::dx12::create_draw_device();
        ::residency_fence = std::make_unique<ff::dx12::fence>("Memory residency fence", nullptr);
    }

    return true;
}

static void destroy_d3d(bool for_reset)
{
    assert(!::frame_commands);

    ff::dx12::wait_for_idle();

    if (!for_reset)
    {
        ::draw_device.reset();

        for (auto& i : ::cpu_descriptor_allocators)
        {
            i.reset();
        }

        for (auto& i : ::gpu_descriptor_allocators)
        {
            i.reset();
        }

        ::upload_allocator.reset();
        ::readback_allocator.reset();
        ::dynamic_buffer_allocator.reset();
        ::static_buffer_allocator.reset();
        ::texture_allocator.reset();
        ::target_allocator.reset();
        ::queues.reset();
        ::object_cache.reset();
        ::residency_fence.reset();
        ::shader_resources.reset();
    }

    if (::video_memory_change_event)
    {
        ::adapter->UnregisterVideoMemoryBudgetChangeNotification(::video_memory_change_event_cookie);
        ::video_memory_change_event_cookie = 0;
        ::video_memory_info = {};
        ::video_memory_change_event.reset();
    }

    ::supports_create_heap_not_resident_ = false;
    ::supports_mesh_shaders_ = false;
    ::simulate_device_invalid = false;
    ::outputs_hash = 0;
    ::adapter.Reset();
    ::device.Reset();
}

bool ff::dx12::init_globals(const ff::dxgi::host_functions& host_functions, D3D_FEATURE_LEVEL feature_level)
{
    ::host_functions = &host_functions;
    ::feature_level = feature_level;

    assert_ret_val(::init_dxgi(), false);
    assert_ret_val(::init_d3d(false), false);

    return true;
}

void ff::dx12::destroy_globals()
{
    ::destroy_d3d(false);
    ::destroy_dxgi();

    ::feature_level = {};
    ::host_functions = {};
}

void ff::dx12::add_device_child(ff::dxgi::device_child_base* child, ff::dx12::device_reset_priority reset_priority)
{
    child->device_child_reset_priority_ = static_cast<int>(reset_priority);

    std::scoped_lock lock(::device_children_mutex);
    ff::intrusive_list::add_back(::first_device_child, ::last_device_child, child);
}

void ff::dx12::remove_device_child(ff::dxgi::device_child_base* child)
{
    // lock scope
    {
        std::scoped_lock lock(::device_children_mutex);
        ff::intrusive_list::remove(::first_device_child, ::last_device_child, child);
    }

    ::removed_device_child_signal.notify(child);
}

const ff::dxgi::host_functions& ff::dxgi_host()
{
    return *::host_functions;
}

bool ff::dx12::reset_device(bool force)
{
    if (!::factory->IsCurrent())
    {
        ::factory = ::create_factory();
        assert_ret_val(::factory, false);

        if (!force)
        {
            if (::adapters_hash != ::get_adapters_hash(::factory.Get()) ||
                ::outputs_hash != ::get_outputs_hash(::factory.Get(), ::adapter.Get()))
            {
                ff::log::write(ff::log::type::dx12, "DXGI adapters or outputs changed");
                force = true;
            }
        }
    }

    if (!force && !ff::dx12::device_valid())
    {
        ff::log::write(ff::log::type::dx12, "DX12 device was reset/removed");
        force = true;
    }

    bool status = true;

    if (force)
    {
        ff::log::write(ff::log::type::dx12, "Recreating DX12 device");

        static bool resetting = false;
        assert_ret_val(!resetting, false);

        resetting = true;
        ff::scope_exit scope_exit([]()
        {
            resetting = false;
        });

        ::destroy_d3d(true);
        assert_ret_val(::init_d3d(true), false);

        struct device_child_t
        {
            bool operator<(const device_child_t& other) const
            {
                return this->child->device_child_reset_priority_ < other.child->device_child_reset_priority_;
            }

            bool operator==(const device_child_t& other) const
            {
                return this->child->device_child_reset_priority_ == other.child->device_child_reset_priority_;
            }

            ff::dxgi::device_child_base* child;
            void* reset_data;
        };

        std::vector<device_child_t> sorted_children;
        {
            std::scoped_lock lock(::device_children_mutex);

            sorted_children.reserve(ff::intrusive_list::count(::first_device_child));

            for (ff::dxgi::device_child_base* i = ::first_device_child; i; i = i->intrusive_next_)
            {
                sorted_children.push_back(device_child_t{ i, nullptr });
            }
        }

        std::stable_sort(sorted_children.begin(), sorted_children.end());

        ff::signal_connection connection = ::removed_device_child_signal.connect([&sorted_children](ff::dxgi::device_child_base* child)
        {
            for (device_child_t& i : sorted_children)
            {
                if (i.child == child)
                {
                    i.child = nullptr;
                    break;
                }
            }
        });

        ff::frame_allocator allocator;

        for (auto i = sorted_children.rbegin(); i != sorted_children.rend(); i++)
        {
            if (i->child)
            {
                i->reset_data = i->child->before_reset(allocator);
            }
        }

        for (device_child_t& i : sorted_children)
        {
            if (i.child && !i.child->reset(i.reset_data))
            {
                debug_fail_msg("Failed to reset graphics object");
                status = false;
                i.child = nullptr;
            }

            i.reset_data = nullptr;
        }

        for (device_child_t& i : sorted_children)
        {
            if (i.child && !i.child->after_reset())
            {
                debug_fail_msg("Failed to reset graphics object");
                status = false;
                i.child = nullptr;
            }
        }
    }

    return status;
}

void ff::dx12::trim_device()
{
    ff::dx12::wait_for_idle();
    ::object_cache->save();
}

bool ff::dx12::device_valid()
{
    return !::simulate_device_invalid && ::device && ::device->GetDeviceRemovedReason() == S_OK;
}

void ff::dx12::device_fatal_error(std::string_view reason)
{
    ff::log::write(ff::log::type::dx12, "Removing DX12 device after fatal error: ", reason);

    if (ff::dx12::device_valid())
    {
        ::simulate_device_invalid = true;
    }
}

D3D_FEATURE_LEVEL ff::dx12::feature_level()
{
    return ::feature_level;
}

IDXGIFactory4* ff::dx12::factory()
{
    return ::factory.Get();
}

IDXGIAdapter3* ff::dx12::adapter()
{
    return ::adapter.Get();
}

ID3D12Device1* ff::dx12::device()
{
    return ::device.Get();
}

const DXGI_QUERY_VIDEO_MEMORY_INFO& ff::dx12::get_video_memory_info()
{
    return ::video_memory_info;
}

ff::dx12::fence& ff::dx12::residency_fence()
{
    return *::residency_fence;
}

bool ff::dx12::supports_create_heap_not_resident()
{
    return supports_create_heap_not_resident_;
}

bool ff::dx12::supports_mesh_shaders()
{
    return supports_mesh_shaders_;
}

ff::dx12::object_cache& ff::dx12::get_object_cache()
{
    return *::object_cache;
}

ff::dxgi::draw_device_base& ff::dx12::get_draw_device()
{
	return *::draw_device;
}

size_t ff::dx12::frame_count()
{
    return ::frame_count;
}

ff::dxgi::command_context_base& ff::dx12::frame_started()
{
    assert(!::frame_commands);

    ::flush_keep_alive();
    ::update_video_memory_info();
    ff::dx12::direct_queue().begin_event(ff::dx12::gpu_event::render_frame);

    ::frame_commands = ff::dx12::direct_queue().new_commands();
    return *::frame_commands;
}

void ff::dx12::frame_complete()
{
    assert(::frame_commands);

    ::frame_commands.reset();
    ::frame_complete_signal.notify(++::frame_count);
    ff::dx12::direct_queue().end_event();

    ff::dxgi_host().flush_commands();

    if (!ff::dx12::device_valid())
    {
        ff::dx12::reset_device(true);
    }
}

ff::dx12::commands& ff::dx12::frame_commands()
{
    assert(::frame_commands);
    return *::frame_commands;
}

void ff::dx12::wait_for_idle()
{
    ::queues->wait_for_idle();
    ::flush_keep_alive();
}

ff::signal_sink<size_t>& ff::dx12::frame_complete_sink()
{
    return ::frame_complete_signal;
}

void ff::dx12::keep_alive_resource(ff::dx12::resource&& resource, ff::dx12::fence_values&& fence_values)
{
    if (!fence_values.complete())
    {
        std::scoped_lock lock(::keep_alive_mutex);
        ::keep_alive_resources.push_back(std::make_pair(std::move(resource), std::move(fence_values)));
    }
}

ff::dx12::queue& ff::dx12::direct_queue()
{
    return ::queues->direct();
}

ff::dx12::queue& ff::dx12::copy_queue()
{
    return ::queues->copy();
}

ff::dx12::queue& ff::dx12::compute_queue()
{
    return ::queues->compute();
}

ff::dx12::cpu_descriptor_allocator& ff::dx12::cpu_buffer_descriptors()
{
    return *::cpu_descriptor_allocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
}

ff::dx12::cpu_descriptor_allocator& ff::dx12::cpu_sampler_descriptors()
{
    return *::cpu_descriptor_allocators[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER];
}

ff::dx12::cpu_descriptor_allocator& ff::dx12::cpu_target_descriptors()
{
    return *::cpu_descriptor_allocators[D3D12_DESCRIPTOR_HEAP_TYPE_RTV];
}

ff::dx12::cpu_descriptor_allocator& ff::dx12::cpu_depth_descriptors()
{
    return *::cpu_descriptor_allocators[D3D12_DESCRIPTOR_HEAP_TYPE_DSV];
}

ff::dx12::gpu_descriptor_allocator& ff::dx12::gpu_view_descriptors()
{
    return *::gpu_descriptor_allocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
}

ff::dx12::gpu_descriptor_allocator& ff::dx12::gpu_sampler_descriptors()
{
    return *::gpu_descriptor_allocators[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER];
}

ff::dx12::mem_allocator_ring& ff::dx12::upload_allocator()
{
    return *::upload_allocator;
}

ff::dx12::mem_allocator_ring& ff::dx12::readback_allocator()
{
    return *::readback_allocator;
}

ff::dx12::mem_allocator_ring& ff::dx12::dynamic_buffer_allocator()
{
    return *::dynamic_buffer_allocator;
}

ff::dx12::mem_allocator& ff::dx12::static_buffer_allocator()
{
    return *::static_buffer_allocator;
}

ff::dx12::mem_allocator& ff::dx12::texture_allocator()
{
    return *::texture_allocator;
}

ff::dx12::mem_allocator& ff::dx12::target_allocator()
{
    return *::target_allocator;
}

ff::resource_object_provider& ff::internal::dx12::shader_resources()
{
    return *::shader_resources;
}
