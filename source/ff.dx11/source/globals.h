#pragma once

namespace ff::dx11
{
    class device_state;
    class object_cache;

    bool reset(bool force);
    void trim();

    IDXGIFactoryX* factory();
    IDXGIAdapterX* adapter();
    ID3D11DeviceX* device();
    ID3D11DeviceContextX* context();
    D3D_FEATURE_LEVEL feature_level();
    ff::dx11::device_state& get_device_state();
    ff::dx11::object_cache& get_object_cache();
}

namespace ff::internal::dx11
{
    class device_child_base;
    enum class device_reset_priority;

    bool init_globals();
    void destroy_globals();

    void add_device_child(ff::internal::dx11::device_child_base* child, ff::internal::dx11::device_reset_priority reset_priority);
    void remove_device_child(ff::internal::dx11::device_child_base* child);

    size_t fix_sample_count(DXGI_FORMAT format, size_t sample_count);
}
