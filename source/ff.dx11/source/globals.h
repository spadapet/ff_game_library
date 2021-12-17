#pragma once

namespace ff::dx11
{
    class device_state;
    class object_cache;
    enum class device_reset_priority;

    bool reset(bool force);
    void trim();
    void wait_for_idle();

    IDXGIFactoryX* factory();
    IDXGIAdapterX* adapter();
    ID3D11DeviceX* device();
    ID3D11DeviceContextX* context();
    D3D_FEATURE_LEVEL feature_level();
    ff::dx11::device_state& get_device_state();
    ff::dx11::object_cache& get_object_cache();

    bool init_globals();
    void destroy_globals();

    void add_device_child(ff::dxgi::device_child_base* child, ff::dx11::device_reset_priority reset_priority);
    void remove_device_child(ff::dxgi::device_child_base* child);
}
