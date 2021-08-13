#include "pch.h"
#include "dxgi_util.h"
#include "graphics.h"
#include "graphics_child_base.h"
#include "target_window_base.h"

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
static size_t dxgi_adapters_hash;
static size_t dxgi_adapter_outputs_hash;

static std::recursive_mutex graphics_mutex;
static std::vector<ff::internal::graphics_child_base*> graphics_children;
static ff::signal<ff::internal::graphics_child_base*> removed_child;
static ff::target_window_base* defer_full_screen_target;
static std::vector<std::pair<ff::target_window_base*, ff::window_size>> defer_sizes;
static ::defer_flags_t defer_flags;
static ff::signal_connection render_presented_connection;
static ff::signal<uint64_t> render_frame_complete_signal;

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

    return true;
}

static void destroy_dxgi()
{
    ::dxgi_adapters_hash = 0;

    ::write_font_loader.Reset();
    ::write_factory.Reset();
    ::dxgi_factory.Reset();
}

bool ff::internal::graphics::init()
{
#if DXVER == 12
    if (ff::constants::debug_build && ::IsDebuggerPresent())
    {
        Microsoft::WRL::ComPtr<ID3D12DebugX> debug_interface;
        if (SUCCEEDED(::D3D12GetDebugInterface(__uuidof(ID3D12DebugX), &debug_interface)))
        {
            debug_interface->EnableDebugLayer();
        }
    }
#endif

    if (::init_dxgi() && ff::internal::graphics::init_d3d(false))
    {
        ::dxgi_adapter_outputs_hash = ff::internal::get_adapter_outputs_hash(::dxgi_factory.Get(), ff::graphics::dxgi_adapter_for_device());
        return true;
    }

    return false;
}

void ff::internal::graphics::destroy()
{
    ::dxgi_adapter_outputs_hash = 0;

    ff::internal::graphics::destroy_d3d(false);
    ::destroy_dxgi();
}

void ff::internal::graphics::render_frame_complete(ff::target_base* target, uint64_t fence_value)
{
    ::render_frame_complete_signal.notify(fence_value);
}

ff::signal_sink<uint64_t>& ff::internal::graphics::render_frame_complete_sink()
{
    return ::render_frame_complete_signal;
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

bool ff::graphics::reset(bool force)
{
    if (!force)
    {
        if (ff::internal::graphics::d3d_device_disconnected())
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
            else if (::dxgi_adapter_outputs_hash != ff::internal::get_adapter_outputs_hash(dxgi_factory_latest.Get(), ff::graphics::dxgi_adapter_for_device()))
            {
                force = true;
            }
        }
    }

    bool status = true;

    if (force)
    {
        ff::internal::graphics::destroy_d3d(true);

        if (!ff::internal::graphics::init_d3d(true))
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
    ::render_presented_connection = target->render_presented().connect(ff::internal::graphics::render_frame_complete);
}

void ff::graphics::defer::remove_target(ff::target_window_base* target)
{
    std::scoped_lock lock(::graphics_mutex);
    assert(target);

    if (::defer_full_screen_target == target)
    {
        ::defer_full_screen_target = nullptr;
        ::render_presented_connection.disconnect();
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
