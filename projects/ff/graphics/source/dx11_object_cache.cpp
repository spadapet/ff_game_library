#include "pch.h"
#include "dx11_operators.h"
#include "dx11_object_cache.h"

size_t ff::dx11_object_cache::hash_data::operator()(const std::shared_ptr<ff::data_base>& value) const
{
    return (value && value->size()) ? ff::stable_hash_bytes(value->data(), value->size()) : 0;
}

bool ff::dx11_object_cache::equals_data::operator()(const std::shared_ptr<ff::data_base>& lhs, const std::shared_ptr<ff::data_base>& rhs) const
{
    if (!lhs || !rhs)
    {
        return lhs == rhs;
    }

    return lhs->size() == rhs->size() && !std::memcmp(lhs->data(), rhs->data(), lhs->size());
}

ff::dx11_object_cache::dx11_object_cache(ID3D11DeviceX* device)
    : device(device)
{}

ID3D11BlendState* ff::dx11_object_cache::get_blend_state(const D3D11_BLEND_DESC& desc)
{
    auto i = this->blend_states.find(desc);
    if (i == this->blend_states.cend())
    {
        Microsoft::WRL::ComPtr<ID3D11BlendState> state;
        if (FAILED(this->device->CreateBlendState(&desc, &state)))
        {
            assert(false);
            return nullptr;
        }

        i = this->blend_states.try_emplace(desc, state).first;
    }

    return i->second.Get();
}

ID3D11DepthStencilState* ff::dx11_object_cache::get_depth_stencil_state(const D3D11_DEPTH_STENCIL_DESC& desc)
{
    auto i = this->depth_states.find(desc);
    if (i == this->depth_states.cend())
    {
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> state;
        if (FAILED(this->device->CreateDepthStencilState(&desc, &state)))
        {
            assert(false);
            return nullptr;
        }

        i = this->depth_states.try_emplace(desc, state).first;
    }

    return i->second.Get();
}

ID3D11RasterizerState* ff::dx11_object_cache::get_rasterize_state(const D3D11_RASTERIZER_DESC& desc)
{
    auto i = this->raster_states.find(desc);
    if (i == this->raster_states.cend())
    {
        Microsoft::WRL::ComPtr<ID3D11RasterizerState> state;
        if (FAILED(this->device->CreateRasterizerState(&desc, &state)))
        {
            assert(false);
            return nullptr;
        }

        i = this->raster_states.try_emplace(desc, state).first;
    }

    return i->second.Get();
}

ID3D11SamplerState* ff::dx11_object_cache::get_sampler_state(const D3D11_SAMPLER_DESC& desc)
{
    auto i = this->sampler_states.find(desc);
    if (i == this->sampler_states.cend())
    {
        Microsoft::WRL::ComPtr<ID3D11SamplerState> state;
        if (FAILED(this->device->CreateSamplerState(&desc, &state)))
        {
            assert(false);
            return nullptr;
        }

        i = this->sampler_states.try_emplace(desc, state).first;
    }

    return i->second.Get();
}

ID3D11VertexShader* ff::dx11_object_cache::get_vertex_shader(std::string_view resource_name)
{
    ff::auto_resource<ff::resource_file> resource = ff::global_resources::get(resource_name);
    std::shared_ptr<ff::saved_data_base> saved_data = resource.object() ? resource->saved_data() : nullptr;
    std::shared_ptr<ff::data_base> data = saved_data ? saved_data->loaded_data() : nullptr;
    ID3D11VertexShader* value = data ? this->get_vertex_shader(data) : nullptr;

    if (value)
    {
        this->resources.insert(resource.resource());
    }

    return value;
}

ID3D11VertexShader* ff::dx11_object_cache::get_vertex_shader_and_input_layout(std::string_view resource_name, Microsoft::WRL::ComPtr<ID3D11InputLayout>& input_layout, const D3D11_INPUT_ELEMENT_DESC* layout, size_t count)
{
    input_layout = this->get_input_layout(resource_name, layout, count);
    return this->get_vertex_shader(resource_name);
}

ID3D11GeometryShader* ff::dx11_object_cache::get_geometry_shader(std::string_view resource_name)
{
    ff::auto_resource<ff::resource_file> resource = ff::global_resources::get(resource_name);
    std::shared_ptr<ff::saved_data_base> saved_data = resource.object() ? resource->saved_data() : nullptr;
    std::shared_ptr<ff::data_base> data = saved_data ? saved_data->loaded_data() : nullptr;
    ID3D11GeometryShader* value = data ? this->get_geometry_shader(data) : nullptr;

    if (value)
    {
        this->resources.insert(resource.resource());
    }

    return value;
}

ID3D11GeometryShader* ff::dx11_object_cache::get_geometry_shader_stream_output(std::string_view resource_name, const D3D11_SO_DECLARATION_ENTRY* layout, size_t count, size_t vertex_stride)
{
    ff::auto_resource<ff::resource_file> resource = ff::global_resources::get(resource_name);
    std::shared_ptr<ff::saved_data_base> saved_data = resource.object() ? resource->saved_data() : nullptr;
    std::shared_ptr<ff::data_base> data = saved_data ? saved_data->loaded_data() : nullptr;
    ID3D11GeometryShader* value = data ? this->get_geometry_shader_stream_output(data, layout, count, vertex_stride) : nullptr;

    if (value)
    {
        this->resources.insert(resource.resource());
    }

    return value;
}

ID3D11PixelShader* ff::dx11_object_cache::get_pixel_shader(std::string_view resource_name)
{
    ff::auto_resource<ff::resource_file> resource = ff::global_resources::get(resource_name);
    std::shared_ptr<ff::saved_data_base> saved_data = resource.object() ? resource->saved_data() : nullptr;
    std::shared_ptr<ff::data_base> data = saved_data ? saved_data->loaded_data() : nullptr;
    ID3D11PixelShader* value = data ? this->get_pixel_shader(data) : nullptr;

    if (value)
    {
        this->resources.insert(resource.resource());
    }

    return value;
}

ID3D11InputLayout* ff::dx11_object_cache::get_input_layout(std::string_view vertex_shader_resource_name, const D3D11_INPUT_ELEMENT_DESC* layout, size_t count)
{
    ff::auto_resource<ff::resource_file> resource = ff::global_resources::get(vertex_shader_resource_name);
    std::shared_ptr<ff::saved_data_base> saved_data = resource.object() ? resource->saved_data() : nullptr;
    std::shared_ptr<ff::data_base> data = saved_data ? saved_data->loaded_data() : nullptr;
    ID3D11InputLayout* value = data ? this->get_input_layout(data, layout, count) : nullptr;

    if (value)
    {
        this->resources.insert(resource.resource());
    }

    return value;
}

ID3D11VertexShader* ff::dx11_object_cache::get_vertex_shader(const std::shared_ptr<ff::data_base>& shader_data)
{
    ID3D11VertexShader* return_value = nullptr;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> value;

    auto iter = this->shaders.find(shader_data);
    if (iter != this->shaders.cend())
    {
        if (SUCCEEDED(iter->second.As(&value)))
        {
            return_value = value.Get();
        }
    }
    else if (shader_data && SUCCEEDED(this->device->CreateVertexShader(shader_data->data(), shader_data->size(), nullptr, &value)))
    {
        return_value = value.Get();
        this->shaders.try_emplace(shader_data, std::move(value));
    }

    return return_value;
}

ID3D11VertexShader* ff::dx11_object_cache::get_vertex_shader_and_input_layout(const std::shared_ptr<ff::data_base>& shader_data, Microsoft::WRL::ComPtr<ID3D11InputLayout>& input_layout, const D3D11_INPUT_ELEMENT_DESC* layout, size_t count)
{
    input_layout = this->get_input_layout(shader_data, layout, count);
    return this->get_vertex_shader(shader_data);
}

ID3D11GeometryShader* ff::dx11_object_cache::get_geometry_shader(const std::shared_ptr<ff::data_base>& shader_data)
{
    ID3D11GeometryShader* return_value = nullptr;
    Microsoft::WRL::ComPtr<ID3D11GeometryShader> value;

    auto iter = this->shaders.find(shader_data);
    if (iter != this->shaders.cend())
    {
        if (SUCCEEDED(iter->second.As(&value)))
        {
            return_value = value.Get();
        }
    }
    else if (shader_data && SUCCEEDED(this->device->CreateGeometryShader(shader_data->data(), shader_data->size(), nullptr, &value)))
    {
        return_value = value.Get();
        this->shaders.try_emplace(shader_data, std::move(value));
    }

    return return_value;
}

ID3D11GeometryShader* ff::dx11_object_cache::get_geometry_shader_stream_output(const std::shared_ptr<ff::data_base>& shader_data, const D3D11_SO_DECLARATION_ENTRY* layout, size_t count, size_t vertex_stride)
{
    ID3D11GeometryShader* return_value = nullptr;
    Microsoft::WRL::ComPtr<ID3D11GeometryShader> value;
    UINT vertex_stride_2 = static_cast<UINT>(vertex_stride);

    auto iter = this->shaders.find(shader_data);
    if (iter != this->shaders.cend())
    {
        if (SUCCEEDED(iter->second.As(&value)))
        {
            return_value = value.Get();
        }
    }
    else if (shader_data && SUCCEEDED(this->device->CreateGeometryShaderWithStreamOutput(shader_data->data(), shader_data->size(), layout, static_cast<UINT>(count), &vertex_stride_2, 1, 0, nullptr, &value)))
    {
        return_value = value.Get();
        this->shaders.try_emplace(shader_data, std::move(value));
    }

    return return_value;
}

ID3D11PixelShader* ff::dx11_object_cache::get_pixel_shader(const std::shared_ptr<ff::data_base>& shader_data)
{
    ID3D11PixelShader* return_value = nullptr;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> value;

    auto iter = this->shaders.find(shader_data);
    if (iter != this->shaders.cend())
    {
        if (SUCCEEDED(iter->second.As(&value)))
        {
            return_value = value.Get();
        }
    }
    else if (shader_data && SUCCEEDED(this->device->CreatePixelShader(shader_data->data(), shader_data->size(), nullptr, &value)))
    {
        return_value = value.Get();
        this->shaders.try_emplace(shader_data, std::move(value));
    }

    return return_value;
}

ID3D11InputLayout* ff::dx11_object_cache::get_input_layout(const std::shared_ptr<ff::data_base>& shader_data, const D3D11_INPUT_ELEMENT_DESC* layout, size_t count)
{
    ID3D11InputLayout* return_value = nullptr;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> value;
    size_t layout_hash = ff::stable_hash_bytes(layout, sizeof(D3D11_INPUT_ELEMENT_DESC) * count);

    auto iter = this->layouts.find(layout_hash);
    if (iter != this->layouts.cend())
    {
        return_value = iter->second.Get();
    }
    else if (shader_data != nullptr && SUCCEEDED(this->device->CreateInputLayout(layout, static_cast<UINT>(count), shader_data->data(), shader_data->size(), &value)))
    {
        return_value = value.Get();
        this->layouts.try_emplace(layout_hash, std::move(value));
    }

    return return_value;
}
