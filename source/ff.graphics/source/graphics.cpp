#include "pch.h"
#include "graphics.h"

namespace
{
    enum class defer_flags_t
    {
        none = 0,

        full_screen_false = 0x001,
        full_screen_true = 0x002,
        full_screen_bits = 0x00f,

        reset_check = 0x010,
        reset_force = 0x020,
        reset_bits = 0x0f0,

        swap_chain_size = 0x100,
        swap_chain_bits = 0xf00,
    };
}

static Microsoft::WRL::ComPtr<IDWriteFactory7> write_factory;
static Microsoft::WRL::ComPtr<IDWriteInMemoryFontFileLoader> write_font_loader;
static const ff::dxgi::client_functions* client_functions;

static std::mutex graphics_mutex;
static ff::dxgi::target_window_base* defer_full_screen_target;
static std::vector<std::pair<ff::dxgi::target_window_base*, ff::window_size>> defer_sizes;
static ::defer_flags_t defer_flags;

static Microsoft::WRL::ComPtr<IDWriteFactory7> create_write_factory()
{
    Microsoft::WRL::ComPtr<IDWriteFactory7> write_factory;
    return (SUCCEEDED(::DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory7), reinterpret_cast<IUnknown**>(write_factory.GetAddressOf()))))
        ? write_factory : nullptr;
}

static Microsoft::WRL::ComPtr<IDWriteInMemoryFontFileLoader> create_write_font_loader()
{
    Microsoft::WRL::ComPtr<IDWriteInMemoryFontFileLoader> write_font_loader;
    return (SUCCEEDED(::write_factory->CreateInMemoryFontFileLoader(&write_font_loader)) &&
        SUCCEEDED(::write_factory->RegisterFontFileLoader(write_font_loader.Get())))
        ? write_font_loader : nullptr;
}

bool ff::internal::graphics::init(const ff::dxgi::client_functions& client_functions)
{
    ::client_functions = &client_functions;

    if (!(::write_factory = ::create_write_factory()) ||
        !(::write_font_loader = ::create_write_font_loader()))
    {
        assert(false);
        return false;
    }

    return true;
}

void ff::internal::graphics::destroy()
{
    ::write_font_loader.Reset();
    ::write_factory.Reset();
    ::client_functions = nullptr;
}

const ff::dxgi::client_functions& ff::dxgi_client()
{
    return *::client_functions;
}

IDWriteFactory7* ff::graphics::write_factory()
{
    return ::write_factory.Get();
}

IDWriteInMemoryFontFileLoader* ff::graphics::write_font_loader()
{
    return ::write_font_loader.Get();
}

static void flush_graphics_commands()
{
    while (::defer_flags != ::defer_flags_t::none)
    {
        std::unique_lock lock(::graphics_mutex);

        if (ff::flags::has_any(::defer_flags, ::defer_flags_t::full_screen_bits))
        {
            bool full_screen = ff::flags::has(::defer_flags, ::defer_flags_t::full_screen_true);
            ff::dxgi::target_window_base* target = ::defer_full_screen_target;
            ::defer_flags = ff::flags::clear(::defer_flags, ::defer_flags_t::full_screen_bits);
            lock.unlock();

            if (target && target->allow_full_screen())
            {
                target->full_screen(full_screen);
            }
        }
        else if (ff::flags::has_any(::defer_flags, ::defer_flags_t::reset_bits))
        {
            bool force = ff::flags::has(::defer_flags, ::defer_flags_t::reset_force);
            ::defer_flags = ff::flags::clear(::defer_flags, ::defer_flags_t::reset_bits);
            lock.unlock();

            ff::dxgi_client().reset_device(force);
        }
        else if (ff::flags::has_any(::defer_flags, ::defer_flags_t::swap_chain_bits))
        {
            std::vector<std::pair<ff::dxgi::target_window_base*, ff::window_size>> defer_sizes = std::move(::defer_sizes);
            ::defer_flags = ff::flags::clear(::defer_flags, ::defer_flags_t::swap_chain_bits);
            lock.unlock();

            for (const auto& i : defer_sizes)
            {
                i.first->size(i.second);
            }
        }
    }
}

void ff::internal::graphics::on_frame_started(ff::dxgi::command_context_base&)
{}

void ff::internal::graphics::on_frame_complete()
{
    ::flush_graphics_commands();
}

void ff::graphics::defer::set_full_screen_target(ff::dxgi::target_window_base* target)
{
    std::scoped_lock lock(::graphics_mutex);
    assert(!::defer_full_screen_target || !target);
    ::defer_full_screen_target = target;
}

void ff::graphics::defer::remove_target(ff::dxgi::target_window_base* target)
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

void ff::graphics::defer::resize_target(ff::dxgi::target_window_base* target, const ff::window_size& size)
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
    }
}

void ff::graphics::defer::reset_device(bool force)
{
    std::scoped_lock lock(::graphics_mutex);

    ::defer_flags = ff::flags::set(
        ff::flags::clear(::defer_flags, ::defer_flags_t::reset_bits),
        force ? ::defer_flags_t::reset_force : ::defer_flags_t::reset_check);
}

void ff::graphics::defer::full_screen(bool value)
{
    std::scoped_lock lock(::graphics_mutex);

    ::defer_flags = ff::flags::set(
        ff::flags::clear(::defer_flags, ::defer_flags_t::full_screen_bits),
        value ? ::defer_flags_t::full_screen_true : ::defer_flags_t::full_screen_false);
}
