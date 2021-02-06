#include "pch.h"
#include "dxgi_util.h"

static bool software_adapter(IDXGIAdapterX* adapter)
{
    DXGI_ADAPTER_DESC3 desc;
    return adapter && SUCCEEDED(adapter->GetDesc3(&desc)) && (desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE) == DXGI_ADAPTER_FLAG3_SOFTWARE;
}

static Microsoft::WRL::ComPtr<IDXGIAdapterX> fix_adapter(IDXGIFactoryX* dxgi, Microsoft::WRL::ComPtr<IDXGIAdapterX> card)
{
    if (dxgi && ::software_adapter(card.Get()))
    {
        Microsoft::WRL::ComPtr<IDXGIAdapter1> default_adapter;
        if (SUCCEEDED(dxgi->EnumAdapters1(0, &default_adapter)))
        {
            Microsoft::WRL::ComPtr<IDXGIAdapterX> default_adapter_x;
            if (SUCCEEDED(default_adapter.As(&default_adapter_x)))
            {
                card = default_adapter_x;
            }
        }
    }

    DXGI_ADAPTER_DESC desc;
    if (SUCCEEDED(card->GetDesc(&desc)))
    {
        Microsoft::WRL::ComPtr<IDXGIAdapterX> adapter_x;
        if (SUCCEEDED(dxgi->EnumAdapterByLuid(desc.AdapterLuid, __uuidof(IDXGIAdapterX), (void**)&adapter_x)))
        {
            card = adapter_x;
        }
    }

    return card;
}

size_t ff::internal::get_adapters_hash(IDXGIFactoryX* factory)
{
    ff::vector<LUID, 32> luids;

    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    for (UINT i = 0; SUCCEEDED(factory->EnumAdapters(i, &adapter)); i++, adapter.Reset())
    {
        DXGI_ADAPTER_DESC desc;
        if (SUCCEEDED(adapter->GetDesc(&desc)))
        {
            luids.push_back(desc.AdapterLuid);
        }
    }

    return !luids.empty() ? ff::hash_bytes(luids.data(), ff::vector_byte_size(luids)) : 0;
}

size_t ff::internal::get_adapter_outputs_hash(IDXGIFactoryX* dxgi, IDXGIAdapterX* card)
{
    ff::vector<HMONITOR, 32> monitors;
    Microsoft::WRL::ComPtr<IDXGIAdapterX> card_x = ::fix_adapter(dxgi, card);

    Microsoft::WRL::ComPtr<IDXGIOutput> output;
    for (UINT i = 0; SUCCEEDED(card_x->EnumOutputs(i++, &output)); output.Reset())
    {
        DXGI_OUTPUT_DESC desc;
        if (SUCCEEDED(output->GetDesc(&desc)))
        {
            monitors.push_back(desc.Monitor);
        }
    }

    return !monitors.empty() ? ff::hash_bytes(monitors.data(), ff::vector_byte_size(monitors)) : 0;
}

std::vector<Microsoft::WRL::ComPtr<IDXGIOutputX>> ff::internal::get_adapter_outputs(IDXGIFactoryX* dxgi, IDXGIAdapterX* card)
{
    Microsoft::WRL::ComPtr<IDXGIOutput> output;
    std::vector<Microsoft::WRL::ComPtr<IDXGIOutputX>> outputs;
    Microsoft::WRL::ComPtr<IDXGIAdapterX> card_x = ::fix_adapter(dxgi, card);

    for (UINT i = 0; SUCCEEDED(card_x->EnumOutputs(i++, &output)); output.Reset())
    {
        Microsoft::WRL::ComPtr<IDXGIOutputX> output_x;
        if (SUCCEEDED(output.As(&output_x)))
        {
            outputs.push_back(output_x);
        }
    }

    return outputs;
}

// This method determines the rotation between the display device's native orientation and the
// current display orientation.
DXGI_MODE_ROTATION ff::internal::get_display_rotation(DXGI_MODE_ROTATION native_orientation, DXGI_MODE_ROTATION current_orientation)
{
    DXGI_MODE_ROTATION rotation = DXGI_MODE_ROTATION_UNSPECIFIED;

    // Note: NativeOrientation can only be Landscape or Portrait even though
    // the DisplayOrientations enum has other values.
    switch (native_orientation)
    {
        case DXGI_MODE_ROTATION_IDENTITY:
        case DXGI_MODE_ROTATION_ROTATE180:
            switch (current_orientation)
            {
                case DXGI_MODE_ROTATION_IDENTITY:
                    rotation = DXGI_MODE_ROTATION_IDENTITY;
                    break;

                case DXGI_MODE_ROTATION_ROTATE90:
                    rotation = DXGI_MODE_ROTATION_ROTATE270;
                    break;

                case DXGI_MODE_ROTATION_ROTATE180:
                    rotation = DXGI_MODE_ROTATION_ROTATE180;
                    break;

                case DXGI_MODE_ROTATION_ROTATE270:
                    rotation = DXGI_MODE_ROTATION_ROTATE90;
                    break;
            }
            break;

        case DXGI_MODE_ROTATION_ROTATE90:
        case DXGI_MODE_ROTATION_ROTATE270:
            switch (current_orientation)
            {
                case DXGI_MODE_ROTATION_IDENTITY:
                    rotation = DXGI_MODE_ROTATION_ROTATE90;
                    break;

                case DXGI_MODE_ROTATION_ROTATE90:
                    rotation = DXGI_MODE_ROTATION_IDENTITY;
                    break;

                case DXGI_MODE_ROTATION_ROTATE180:
                    rotation = DXGI_MODE_ROTATION_ROTATE270;
                    break;

                case DXGI_MODE_ROTATION_ROTATE270:
                    rotation = DXGI_MODE_ROTATION_ROTATE180;
                    break;
            }
            break;
    }

    return rotation;
}

bool ff::internal::compressed_format(DXGI_FORMAT format)
{
    return DirectX::IsCompressed(format);
}

bool ff::internal::color_format(DXGI_FORMAT format)
{
    return DirectX::IsSRGB(DirectX::MakeSRGB(format));
}

bool ff::internal::palette_format(DXGI_FORMAT format)
{
    return format == DXGI_FORMAT_R8_UINT;
}

bool ff::internal::has_alpha(DXGI_FORMAT format)
{
    return DirectX::HasAlpha(format);
}

bool ff::internal::supports_pre_multiplied_alpha(DXGI_FORMAT format)
{
    return !ff::internal::compressed_format(format) && ff::internal::color_format(format) && ff::internal::has_alpha(format);
}

DXGI_FORMAT ff::internal::parse_texture_format(std::string_view format_name)
{
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

    if (format_name == "rgba32")
    {
        format = DXGI_FORMAT_R8G8B8A8_UNORM;
    }
    else if (format_name == "bgra32")
    {
        format = DXGI_FORMAT_B8G8R8A8_UNORM;
    }
    else if (format_name == "bc1")
    {
        format = DXGI_FORMAT_BC1_UNORM;
    }
    else if (format_name == "bc2")
    {
        format = DXGI_FORMAT_BC2_UNORM;
    }
    else if (format_name == "bc3")
    {
        format = DXGI_FORMAT_BC3_UNORM;
    }
    else if (format_name == "pal" || format_name == "palette")
    {
        format = DXGI_FORMAT_R8_UINT;
    }
    else if (format_name == "gray")
    {
        format = DXGI_FORMAT_R8_UNORM;
    }
    else if (format_name == "bw")
    {
        format = DXGI_FORMAT_R1_UNORM;
    }
    else if (format_name == "alpha")
    {
        format = DXGI_FORMAT_A8_UNORM;
    }

    assert(format != DXGI_FORMAT_UNKNOWN);
    return format;
}
