#include "pch.h"
#include "write/write.h"

static Microsoft::WRL::ComPtr<IDWriteFactory7> write_factory;
static Microsoft::WRL::ComPtr<IDWriteInMemoryFontFileLoader> write_font_loader;

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

bool ff::internal::write::init()
{
    if (!(::write_factory = ::create_write_factory()) ||
        !(::write_font_loader = ::create_write_font_loader()))
    {
        debug_fail_ret_val(false);
    }

    return true;
}

void ff::internal::write::destroy()
{
    ::write_font_loader.Reset();
    ::write_factory.Reset();
}

IDWriteFactory7* ff::write_factory()
{
    return ::write_factory.Get();
}

IDWriteInMemoryFontFileLoader* ff::write_font_loader()
{
    return ::write_font_loader.Get();
}
