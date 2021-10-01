#include "pch.h"
#include "graphics.h"
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

static Microsoft::WRL::ComPtr<IDWriteFactoryX> write_factory;
static Microsoft::WRL::ComPtr<IDWriteInMemoryFontFileLoader> write_font_loader;

static std::mutex graphics_mutex;
static ff::target_window_base* defer_full_screen_target;
static std::vector<std::pair<ff::target_window_base*, ff::window_size>> defer_sizes;
static ::defer_flags_t defer_flags;

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

bool ff::internal::graphics::init()
{
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
}

IDWriteFactoryX* ff::graphics::write_factory()
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

            ff_dx::reset(force);
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
