#pragma once

namespace ff
{
    class dx11_object_cache
    {
    public:
        dx11_object_cache(ID3D11DeviceX* device);
        dx11_object_cache(dx11_object_cache&& other) noexcept = delete;
        dx11_object_cache(const dx11_object_cache& other) = delete;

        dx11_object_cache& operator=(dx11_object_cache&& other) noexcept = delete;
        dx11_object_cache& operator=(const dx11_object_cache& other) = delete;

        ID3D11BlendState* get_blend_state(const D3D11_BLEND_DESC& desc);
        ID3D11DepthStencilState* get_depth_stencil_state(const D3D11_DEPTH_STENCIL_DESC& desc);
        ID3D11RasterizerState* get_rasterize_state(const D3D11_RASTERIZER_DESC& desc);
        ID3D11SamplerState* get_sampler_state(const D3D11_SAMPLER_DESC& desc);

        ID3D11VertexShader* get_vertex_shader(ff::resource_object_provider& resources, std::string_view resource_name);
        ID3D11VertexShader* get_vertex_shader_and_input_layout(ff::resource_object_provider& resources, std::string_view resource_name, Microsoft::WRL::ComPtr<ID3D11InputLayout>& input_layout, const D3D11_INPUT_ELEMENT_DESC* layout, size_t count);
        ID3D11GeometryShader* get_geometry_shader(ff::resource_object_provider& resources, std::string_view resource_name);
        ID3D11GeometryShader* get_geometry_shader_stream_output(ff::resource_object_provider& resources, std::string_view resource_name, const D3D11_SO_DECLARATION_ENTRY* layout, size_t count, size_t vertex_stride);
        ID3D11PixelShader* get_pixel_shader(ff::resource_object_provider& resources, std::string_view resource_name);
        ID3D11InputLayout* get_input_layout(ff::resource_object_provider& resources, std::string_view vertex_shader_resource_name, const D3D11_INPUT_ELEMENT_DESC* layout, size_t count);

        ID3D11VertexShader* get_vertex_shader(const std::shared_ptr<ff::data_base>& shader_data);
        ID3D11VertexShader* get_vertex_shader_and_input_layout(const std::shared_ptr<ff::data_base>& shader_data, Microsoft::WRL::ComPtr<ID3D11InputLayout>& input_layout, const D3D11_INPUT_ELEMENT_DESC* layout, size_t count);
        ID3D11GeometryShader* get_geometry_shader(const std::shared_ptr<ff::data_base>& shader_data);
        ID3D11GeometryShader* get_geometry_shader_stream_output(const std::shared_ptr<ff::data_base>& shader_data, const D3D11_SO_DECLARATION_ENTRY* layout, size_t count, size_t vertex_stride);
        ID3D11PixelShader* get_pixel_shader(const std::shared_ptr<ff::data_base>& shader_data);
        ID3D11InputLayout* get_input_layout(const std::shared_ptr<ff::data_base>& shader_data, const D3D11_INPUT_ELEMENT_DESC* layout, size_t count);

    private:
        Microsoft::WRL::ComPtr<ID3D11DeviceX> device;

        struct hash_data
        {
            size_t operator()(const std::shared_ptr<ff::data_base>& value) const;
        };

        struct equals_data
        {
            bool operator()(const std::shared_ptr<ff::data_base>& lhs, const std::shared_ptr<ff::data_base>& rhs) const;
        };

        std::unordered_map<D3D11_BLEND_DESC, Microsoft::WRL::ComPtr<ID3D11BlendState>, ff::hash<D3D11_BLEND_DESC>> blend_states;
        std::unordered_map<D3D11_DEPTH_STENCIL_DESC, Microsoft::WRL::ComPtr<ID3D11DepthStencilState>, ff::hash<D3D11_DEPTH_STENCIL_DESC>> depth_states;
        std::unordered_map<D3D11_RASTERIZER_DESC, Microsoft::WRL::ComPtr<ID3D11RasterizerState>, ff::hash<D3D11_RASTERIZER_DESC>> raster_states;
        std::unordered_map<D3D11_SAMPLER_DESC, Microsoft::WRL::ComPtr<ID3D11SamplerState>, ff::hash<D3D11_SAMPLER_DESC>> sampler_states;
        std::unordered_map<std::shared_ptr<ff::data_base>, Microsoft::WRL::ComPtr<IUnknown>, typename hash_data, typename equals_data> shaders;
        std::unordered_map<size_t, Microsoft::WRL::ComPtr<ID3D11InputLayout>, ff::no_hash<size_t>> layouts;
        std::unordered_set<std::shared_ptr<ff::resource>> resources;
    };
}
