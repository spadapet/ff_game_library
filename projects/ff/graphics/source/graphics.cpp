#include "pch.h"
#include "dx11_device_state.h"
#include "dx11_object_cache.h"
#include "dx11_target_window_base.h"
#include "dxgi_util.h"
#include "graphics.h"
#include "graphics_child_base.h"

namespace
{
    enum class defer_flags_t
    {
        none = 0,

        full_screen_false = 0x001,
        full_screen_true = 0x002,
        full_screen_bits = 0x00f,

        validate_check = 0x010,
        validate_force = 0x020,
        validate_bits = 0x0f0,

        swap_chain_size = 0x100,
        swap_chain_bits = 0xf00,
    };
}

static Microsoft::WRL::ComPtr<IDXGIFactoryX> dxgi_factory;
static Microsoft::WRL::ComPtr<IDWriteFactoryX> write_factory;
static Microsoft::WRL::ComPtr<IDWriteInMemoryFontFileLoader> write_font_loader;
static Microsoft::WRL::ComPtr<IDXGIDeviceX> dxgi_device;
static Microsoft::WRL::ComPtr<IDXGIFactoryX> dxgi_factory_for_device;
static Microsoft::WRL::ComPtr<IDXGIAdapterX> dxgi_adapter_for_device;
static size_t dxgi_adapters_hash;
static size_t dxgi_adapter_outputs_hash;

static Microsoft::WRL::ComPtr<ID3D11DeviceX> dx11_device;
static Microsoft::WRL::ComPtr<ID3D11DeviceContextX> dx11_device_context;
static std::unique_ptr<ff::dx11_object_cache> dx11_object_cache;
static std::unique_ptr<ff::dx11_device_state> dx11_device_state;
static D3D_FEATURE_LEVEL dx_feature_level;

static std::recursive_mutex graphics_mutex;
static std::vector<ff::internal::graphics_child_base*> graphics_children;
static ff::signal<ff::internal::graphics_child_base*> removed_child;
static ff::target_window_base* defer_full_screen_target;
static std::vector<std::pair<ff::target_window_base*, ff::window_size>> defer_sizes;
static ::defer_flags_t defer_flags;

static Microsoft::WRL::ComPtr<IDXGIFactoryX> create_dxgi_factory()
{
    UINT flags = (ff::constants::debug_build && ::IsDebuggerPresent()) ? DXGI_CREATE_FACTORY_DEBUG : 0;

    Microsoft::WRL::ComPtr<IDXGIFactoryX> dxgi_factory;
    return (SUCCEEDED(::CreateDXGIFactory2(flags, __uuidof(IDXGIFactoryX), reinterpret_cast<void**>(dxgi_factory.GetAddressOf()))))
        ? dxgi_factory : nullptr;
}

static Microsoft::WRL::ComPtr<IDWriteFactoryX> create_write_factory()
{
    Microsoft::WRL::ComPtr<IDWriteFactoryX> write_factory;
    return (SUCCEEDED(::DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactoryX), reinterpret_cast<IUnknown**>(write_factory.GetAddressOf()))))
        ? write_factory : nullptr;
}

static Microsoft::WRL::ComPtr<IDWriteInMemoryFontFileLoader> create_write_font_loader()
{
    Microsoft::WRL::ComPtr<IDWriteInMemoryFontFileLoader> write_font_loader;
    return (SUCCEEDED(::write_factory->CreateInMemoryFontFileLoader(&write_font_loader)) &&
        SUCCEEDED(::write_factory->RegisterFontFileLoader(write_font_loader.Get())))
        ? write_font_loader : nullptr;
}

static Microsoft::WRL::ComPtr<ID3D11DeviceX> create_dx11_device(D3D_FEATURE_LEVEL& feature_level, Microsoft::WRL::ComPtr<ID3D11DeviceContextX>& dx11_device_context)
{
    Microsoft::WRL::ComPtr<ID3D11Device> created_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceX> created_device_x;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> created_device_context;

    std::array<D3D_FEATURE_LEVEL, 4> feature_levels
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    HRESULT hr = E_FAIL;
    const bool allow_debug = ff::constants::debug_build && ::IsDebuggerPresent();

    for (int create_hardware_device = 1; FAILED(hr) && create_hardware_device >= 0; create_hardware_device--)
    {
        for (int create_debug_device = allow_debug ? 1 : 0; FAILED(hr) && create_debug_device >= 0; create_debug_device--)
        {
            hr = ::D3D11CreateDevice(
                nullptr, // adapter
                create_hardware_device ? D3D_DRIVER_TYPE_HARDWARE : D3D_DRIVER_TYPE_WARP,
                nullptr, // software module
                D3D11_CREATE_DEVICE_BGRA_SUPPORT | (create_debug_device ? D3D11_CREATE_DEVICE_DEBUG : 0),
                feature_levels.data(),
                static_cast<UINT>(feature_levels.size()),
                D3D11_SDK_VERSION,
                created_device.GetAddressOf(),
                &feature_level,
                created_device_context.GetAddressOf());
        }
    }

    if (SUCCEEDED(hr) &&
        SUCCEEDED(created_device.As(&created_device_x)) &&
        SUCCEEDED(created_device_context.As(&dx11_device_context)))
    {
        return created_device_x;
    }

    return nullptr;
}

static bool init_dxgi()
{
    if (!(::dxgi_factory = ::create_dxgi_factory()) ||
        !(::write_factory = ::create_write_factory()) ||
        !(::write_font_loader = ::create_write_font_loader()))
    {
        assert(false);
        return false;
    }

    ::dxgi_adapters_hash = ff::internal::get_adapters_hash(::dxgi_factory.Get());
    ::dxgi_adapter_outputs_hash = ff::internal::get_adapter_outputs_hash(::dxgi_factory.Get(), ::dxgi_adapter_for_device.Get());

    return true;
}

static void destroy_dxgi()
{
    ::dxgi_adapters_hash = 0;
    ::dxgi_adapter_outputs_hash = 0;

    ::write_font_loader.Reset();
    ::write_factory.Reset();
    ::dxgi_adapter_for_device.Reset();
    ::dxgi_factory_for_device.Reset();
    ::dxgi_device.Reset();
    ::dxgi_factory.Reset();
}

static bool init_dx11()
{
    if (!(::dx11_device = ::create_dx11_device(::dx_feature_level, ::dx11_device_context)) ||
        FAILED(::dx11_device.As(&::dxgi_device)) ||
        FAILED(::dxgi_device->SetMaximumFrameLatency(1)) ||
        FAILED(::dxgi_device->GetParent(__uuidof(IDXGIAdapterX), reinterpret_cast<void**>(::dxgi_adapter_for_device.GetAddressOf()))) ||
        FAILED(::dxgi_adapter_for_device->GetParent(__uuidof(IDXGIFactoryX), reinterpret_cast<void**>(::dxgi_factory_for_device.GetAddressOf()))))
    {
        assert(false);
        return false;
    }

    ::dx11_object_cache = std::make_unique<ff::dx11_object_cache>(::dx11_device.Get());
    ::dx11_device_state = std::make_unique<ff::dx11_device_state>(::dx11_device_context.Get());

    return true;
}

static void destroy_dx11()
{
    ::dx_feature_level = static_cast<D3D_FEATURE_LEVEL>(0);
    ::dx11_device_state->clear();

    ::dx11_device_state.reset();
    ::dx11_object_cache.reset();

    ::dx11_device_context.Reset();
    ::dx11_device.Reset();
}

bool ff::internal::graphics::init()
{
    return ::init_dxgi() && ::init_dx11();
}

void ff::internal::graphics::destroy()
{
    ::destroy_dx11();
    ::destroy_dxgi();
}

void ff::internal::graphics::add_child(ff::internal::graphics_child_base* child)
{
    std::scoped_lock lock(::graphics_mutex);
    ::graphics_children.push_back(child);
}

void ff::internal::graphics::remove_child(ff::internal::graphics_child_base* child)
{
    std::scoped_lock lock(::graphics_mutex);
    auto i = std::find(::graphics_children.cbegin(), ::graphics_children.cend(), child);
    if (i != ::graphics_children.cend())
    {
        ::graphics_children.erase(i);
        ::removed_child.notify(child);
    }
}

IDXGIFactoryX* ff::graphics::dxgi_factory()
{
    if (!::dxgi_factory->IsCurrent())
    {
        ::dxgi_factory = ::create_dxgi_factory();
    }

    return ::dxgi_factory.Get();
}

IDWriteFactoryX* ff::graphics::write_factory()
{
    return ::write_factory.Get();
}

IDWriteInMemoryFontFileLoader* ff::graphics::write_font_loader()
{
    return ::write_font_loader.Get();
}

IDXGIDeviceX* ff::graphics::dxgi_device()
{
    return ::dxgi_device.Get();
}

IDXGIFactoryX* ff::graphics::dxgi_factory_for_device()
{
    return ::dxgi_factory_for_device.Get();
}

IDXGIAdapterX* ff::graphics::dxgi_adapter_for_device()
{
    return ::dxgi_adapter_for_device.Get();
}

ID3D11DeviceX* ff::graphics::dx11_device()
{
    return ::dx11_device.Get();
}

ID3D11DeviceContextX* ff::graphics::dx11_device_context()
{
    return ::dx11_device_context.Get();
}

D3D_FEATURE_LEVEL ff::graphics::dx_feature_level()
{
    return ::dx_feature_level;
}

ff::dx11_device_state& ff::graphics::dx11_device_state()
{
    return *::dx11_device_state;
}

ff::dx11_object_cache& ff::graphics::dx11_object_cache()
{
    return *::dx11_object_cache;
}

bool ff::graphics::reset(bool force)
{
    if (!force)
    {
        if (FAILED(::dx11_device->GetDeviceRemovedReason()))
        {
            force = true;
        }
        else if (!::dxgi_factory->IsCurrent())
        {
            Microsoft::WRL::ComPtr<IDXGIFactoryX> dxgi_factory_latest = ff::graphics::dxgi_factory();

            if (::dxgi_adapters_hash != ff::internal::get_adapters_hash(dxgi_factory_latest.Get()))
            {
                force = true;
            }
            else if (::dxgi_adapter_outputs_hash != ff::internal::get_adapter_outputs_hash(dxgi_factory_latest.Get(), ::dxgi_adapter_for_device.Get()))
            {
                force = true;
            }
        }
    }

    bool status = true;

    if (force)
    {
        ::destroy_dx11();

        if (!::init_dx11())
        {
            return false;
        }

        std::vector<ff::internal::graphics_child_base*> sorted_children;
        {
            std::scoped_lock lock(::graphics_mutex);
            sorted_children = ::graphics_children;
        }

        std::stable_sort(sorted_children.begin(), sorted_children.end(),
            [](const ff::internal::graphics_child_base* lhs, const ff::internal::graphics_child_base* rhs)
            {
                return lhs->reset_priority() > rhs->reset_priority();
            });

        ff::signal_connection connection = ::removed_child.connect([&sorted_children](ff::internal::graphics_child_base* child)
            {
                auto i = std::find(sorted_children.begin(), sorted_children.end(), child);
                if (i != sorted_children.end())
                {
                    *i = nullptr;
                }
            });

        for (ff::internal::graphics_child_base* child : sorted_children)
        {
            if (child && !child->reset())
            {
                status = false;
            }
        }
    }

    assert(status);
    return status;
}

static void flush_graphics_commands()
{
    while (::defer_flags != ::defer_flags_t::none)
    {
        std::unique_lock lock(::graphics_mutex);

        if (ff::flags::has_any(::defer_flags, ::defer_flags_t::full_screen_bits))
        {
            bool full_screen = ff::flags::has(::defer_flags, ::defer_flags_t::full_screen_true);
            ff::target_window_base* target = ::defer_full_screen_target;
            ::defer_flags = ff::flags::clear(::defer_flags, ::defer_flags_t::full_screen_bits);
            lock.unlock();

            if (target && target->allow_full_screen())
            {
                target->full_screen(full_screen);
            }
        }
        else if (ff::flags::has_any(::defer_flags, ::defer_flags_t::validate_bits))
        {
            bool force = ff::flags::has(::defer_flags, ::defer_flags_t::validate_force);
            ::defer_flags = ff::flags::clear(::defer_flags, ::defer_flags_t::validate_bits);
            lock.unlock();

            ff::graphics::reset(force);
        }
        else if (ff::flags::has_any(::defer_flags, ::defer_flags_t::swap_chain_bits))
        {
            std::vector<std::pair<ff::target_window_base*, ff::window_size>> defer_sizes = std::move(::defer_sizes);
            ::defer_flags = ff::flags::clear(::defer_flags, ::defer_flags_t::swap_chain_bits);
            lock.unlock();

            for (const auto& i : defer_sizes)
            {
                i.first->size(i.second);
            }
        }
    }
}

static void post_flush_graphics_commands()
{
    ff::thread_dispatch::get_game()->post(::flush_graphics_commands);
}

void ff::graphics::defer::set_full_screen_target(ff::target_window_base* target)
{
    std::scoped_lock lock(::graphics_mutex);
    assert(!::defer_full_screen_target || !target);
    ::defer_full_screen_target = target;
}

void ff::graphics::defer::remove_target(ff::target_window_base* target)
{
    std::scoped_lock lock(::graphics_mutex);
    assert(target);

    if (::defer_full_screen_target == target)
    {
        ::defer_full_screen_target = nullptr;
    }

    for (auto i = ::defer_sizes.cbegin(); i != ::defer_sizes.cend(); i++)
    {
        if (i->first == target)
        {
            ::defer_sizes.erase(i);
            break;
        }
    }
}

void ff::graphics::defer::resize_target(ff::target_window_base* target, const ff::window_size& size)
{
    assert(target);
    if (target)
    {
        std::scoped_lock lock(::graphics_mutex);

        ::defer_flags = ff::flags::set(
            ff::flags::clear(::defer_flags, ::defer_flags_t::swap_chain_bits),
            ::defer_flags_t::swap_chain_size);

        bool found_match = false;
        for (auto& i : ::defer_sizes)
        {
            if (i.first == target)
            {
                i.second = size;
                found_match = true;
                break;
            }
        }

        if (!found_match)
        {
            ::defer_sizes.push_back(std::make_pair(target, size));
        }

        ::post_flush_graphics_commands();
    }
}

void ff::graphics::defer::validate_device(bool force)
{
    std::scoped_lock lock(::graphics_mutex);

    ::defer_flags = ff::flags::set(
        ff::flags::clear(::defer_flags, ::defer_flags_t::validate_bits),
        force ? ::defer_flags_t::validate_force : ::defer_flags_t::validate_check);

    ::post_flush_graphics_commands();
}

void ff::graphics::defer::full_screen(bool value)
{
    std::scoped_lock lock(::graphics_mutex);

    ::defer_flags = ff::flags::set(
        ff::flags::clear(::defer_flags, ::defer_flags_t::full_screen_bits),
        value ? ::defer_flags_t::full_screen_true : ::defer_flags_t::full_screen_false);

    ::post_flush_graphics_commands();
}
