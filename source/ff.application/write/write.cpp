#include "pch.h"
#include "write/write.h"

static std::mutex write_lock;
static Microsoft::WRL::ComPtr<IDWriteFactory7> write_factory;
static Microsoft::WRL::ComPtr<IDWriteInMemoryFontFileLoader> write_font_loader;

static Microsoft::WRL::ComPtr<IDWriteFactory7> create_write_factory()
{
    Microsoft::WRL::ComPtr<IDWriteFactory7> write_factory;
    assert_hr_ret_val(::DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory7), reinterpret_cast<IUnknown**>(write_factory.GetAddressOf())), nullptr);
    return write_factory;
}

static Microsoft::WRL::ComPtr<IDWriteInMemoryFontFileLoader> create_write_font_loader(IDWriteFactory7* factory)
{
    Microsoft::WRL::ComPtr<IDWriteInMemoryFontFileLoader> write_font_loader;
    assert_hr_ret_val(factory->CreateInMemoryFontFileLoader(&write_font_loader), nullptr);
    assert_hr_ret_val(factory->RegisterFontFileLoader(write_font_loader.Get()), nullptr);
    return write_font_loader;
}

bool ff::internal::write::init()
{
    // Delay-create the write factory and font loader since they are rarely needed
    return true;
}

void ff::internal::write::destroy()
{
    std::scoped_lock lock(::write_lock);
    ::write_font_loader.Reset();
    ::write_factory.Reset();
}

IDWriteFactory7* ff::write_factory()
{
    if (!::write_factory)
    {
        std::scoped_lock lock(::write_lock);
        if (!::write_factory)
        {
            ::write_factory = ::create_write_factory();
        }
    }

    return ::write_factory.Get();
}

IDWriteInMemoryFontFileLoader* ff::write_font_loader()
{
    if (!::write_font_loader)
    {
        Microsoft::WRL::ComPtr<IDWriteFactory7> factory = ff::write_factory();

        std::scoped_lock lock(::write_lock);
        if (!::write_font_loader)
        {
            ::write_font_loader = ::create_write_font_loader(factory.Get());
        }
    }

    return ::write_font_loader.Get();
}
