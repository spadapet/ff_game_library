#pragma once

namespace ff
{
    class dx11_device_state;
    class dx11_object_cache;
}

namespace ff::internal
{
    class graphics_child_base;
}

namespace ff::graphics
{
    bool reset(bool force);
}

namespace ff::graphics::internal
{
    bool init();
    void destroy();

    void add_child(ff::internal::graphics_child_base* child);
    void remove_child(ff::internal::graphics_child_base* child);

    IDXGIFactoryX* dxgi_factory();
    IDWriteFactoryX* write_factory();
    IDWriteInMemoryFontFileLoader* write_font_loader();

    IDXGIDeviceX* dxgi_device();
    IDXGIFactoryX* dxgi_factory_for_device();
    IDXGIAdapterX* dxgi_adapter_for_device();
    ID3D11DeviceX* dx11_device();
    ID3D11DeviceContextX* dx11_device_context();
    D3D_FEATURE_LEVEL dx11_feature_level();

    ff::dx11_device_state& dx11_device_state();
    ff::dx11_object_cache& dx11_object_cache();
}
