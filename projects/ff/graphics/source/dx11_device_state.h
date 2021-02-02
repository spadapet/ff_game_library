#pragma once

#include "graphics_counters.h"

namespace ff
{
    class dx11_device_state
    {
    public:
        dx11_device_state();
        dx11_device_state(ID3D11DeviceContext* context);
        dx11_device_state(dx11_device_state&& other) noexcept = delete;
        dx11_device_state(const dx11_device_state& other) = delete;

        dx11_device_state& operator=(dx11_device_state&& other) noexcept = delete;
        dx11_device_state& operator=(const dx11_device_state& other) = delete;

        void clear();
        void reset(ID3D11DeviceContext* context);
        void apply(dx11_device_state& dest);
        ff::graphics_counters reset_counters();
        void draw(size_t count, size_t start);
        void draw_indexed(size_t index_count, size_t index_start, int vertex_offset);
        void* map(ID3D11Resource* buffer, D3D11_MAP type, D3D11_MAPPED_SUBRESOURCE* map = nullptr);
        void unmap(ID3D11Resource* buffer);
        void update_discard(ID3D11Resource* buffer, const void* data, size_t size);
        void clear_render_target(ID3D11RenderTargetView* view, const DirectX::XMFLOAT4& color);
        void clear_depth_stencil(ID3D11DepthStencilView* view, bool clear_depth, bool clear_stencil, float depth, BYTE stencil);
        void update_subresource(ID3D11Resource* dest, UINT dest_subresource, const D3D11_BOX* dest_box, const void* src_data, UINT src_row_pitch, UINT src_depth_pitch);
        void copy_subresource_region(ID3D11Resource* dest_resource, UINT dest_subresource, UINT dest_x, UINT dest_y, UINT dest_z, ID3D11Resource* srcResource, UINT src_subresource, const D3D11_BOX* src_box);

        void set_vertex_ia(ID3D11Buffer* value, size_t stride, size_t offset);
        void set_index_ia(ID3D11Buffer* value, DXGI_FORMAT format, size_t offset);
        void set_layout_ia(ID3D11InputLayout* value);
        void set_topology_ia(D3D_PRIMITIVE_TOPOLOGY value);
        void set_append_so(ID3D11Buffer* value);
        void set_output_so(ID3D11Buffer* value, size_t offset);

        void set_vs(ID3D11VertexShader* value);
        void set_samplers_vs(ID3D11SamplerState* const* values, size_t start, size_t count);
        void set_constants_vs(ID3D11Buffer* const* values, size_t start, size_t count);
        void set_resources_vs(ID3D11ShaderResourceView* const* values, size_t start, size_t count);

        void set_gs(ID3D11GeometryShader* value);
        void set_samplers_gs(ID3D11SamplerState* const* values, size_t start, size_t count);
        void set_constants_gs(ID3D11Buffer* const* values, size_t start, size_t count);
        void set_resources_gs(ID3D11ShaderResourceView* const* values, size_t start, size_t count);

        void set_ps(ID3D11PixelShader* value);
        void set_samplers_ps(ID3D11SamplerState* const* value, size_t start, size_t count);
        void set_constants_ps(ID3D11Buffer* const* values, size_t start, size_t count);
        void set_resources_ps(ID3D11ShaderResourceView* const* values, size_t start, size_t count);

        void set_blend(ID3D11BlendState* value, const DirectX::XMFLOAT4& blend_factor, UINT sample_mask);
        void set_depth(ID3D11DepthStencilState* value, UINT stencil);
        void set_targets(ID3D11RenderTargetView* const* targets, size_t count, ID3D11DepthStencilView* depth);
        void set_raster(ID3D11RasterizerState* value);
        void set_viewports(const D3D11_VIEWPORT* value, size_t count);
        void set_scissors(const D3D11_RECT* value, size_t count);
        const Microsoft::WRL::ComPtr<ID3D11DepthStencilView>& depth_view() const;

    private:
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
        ff::graphics_counters counters;

        template<typename TShader>
        struct shader_state
        {
            void reset()
            {
                this->shader.Reset();

                for (auto& value : this->samplers)
                {
                    value.Reset();
                }

                for (auto& value : this->constants)
                {
                    value.Reset();
                }

                for (auto& value : this->resources)
                {
                    value.Reset();
                }
            }

            Microsoft::WRL::ComPtr<TShader> shader;
            std::array<Microsoft::WRL::ComPtr<ID3D11SamplerState>, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT> samplers;
            std::array<Microsoft::WRL::ComPtr<ID3D11Buffer>, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT> constants;
            std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT> resources;
        };

        // Shaders
        shader_state<ID3D11VertexShader> vs_state;
        shader_state<ID3D11GeometryShader> gs_state;
        shader_state<ID3D11PixelShader> ps_state;

        // Input assembler
        Microsoft::WRL::ComPtr<ID3D11Buffer> ia_index;
        DXGI_FORMAT ia_index_format;
        UINT ia_index_offset;
        Microsoft::WRL::ComPtr<ID3D11InputLayout> ia_layout;
        D3D_PRIMITIVE_TOPOLOGY ia_topology;
        std::array<Microsoft::WRL::ComPtr<ID3D11Buffer>, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT> ia_vertexes;
        std::array<UINT, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT> ia_vertex_strides;
        std::array<UINT, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT> ia_vertex_offsets;

        // Output merger
        UINT blend_sample_mask;
        UINT depth_stencil;
        size_t target_views_count;
        DirectX::XMFLOAT4 blend_factor;
        Microsoft::WRL::ComPtr<ID3D11BlendState> blend;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depth_state;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depth_view_;
        std::array<Microsoft::WRL::ComPtr<ID3D11RenderTargetView>, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> target_views;

        // Raster state
        Microsoft::WRL::ComPtr<ID3D11RasterizerState> raster;
        std::array<D3D11_VIEWPORT, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> viewports;
        std::array<D3D11_RECT, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> scissors;
        size_t viewports_count;
        size_t scissor_count;

        // Stream out
        std::array<Microsoft::WRL::ComPtr<ID3D11Buffer>, D3D11_SO_STREAM_COUNT> so_targets;
    };
}
