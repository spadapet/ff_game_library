#include "pch.h"
#include "dx_operators.h"
#include "dx11_device_state.h"

ff::dx11_device_state::dx11_device_state()
{
    this->clear();
}

ff::dx11_device_state::dx11_device_state(ID3D11DeviceContext* context)
{
    this->reset(context);
}

void ff::dx11_device_state::clear()
{
    if (this->context)
    {
        this->context->ClearState();
        this->context->Flush();
    }

    this->counters = ff::graph_counters{};

    this->vs_state.reset();
    this->gs_state.reset();
    this->ps_state.reset();

    this->ia_index.Reset();
    this->ia_index_format = DXGI_FORMAT_UNKNOWN;
    this->ia_index_offset = 0;
    this->ia_layout.Reset();
    this->ia_topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;

    this->blend.Reset();
    this->blend_factor = DirectX::XMFLOAT4(1, 1, 1, 1);
    this->blend_sample_mask = 0xFFFFFFFF;
    this->depth_state.Reset();
    this->depth_stencil = 0;
    this->depth_view_.Reset();
    this->raster.Reset();
    this->scissor_count = 0;
    this->target_views_count = 0;
    this->viewports_count = 0;

    std::memset(this->ia_vertex_strides.data(), 0, ff::array_byte_size(this->ia_vertex_strides));
    std::memset(this->ia_vertex_offsets.data(), 0, ff::array_byte_size(this->ia_vertex_offsets));
    std::memset(this->viewports.data(), 0, ff::array_byte_size(this->viewports));
    std::memset(this->scissors.data(), 0, ff::array_byte_size(this->scissors));

    for (auto& value : this->ia_vertexes)
    {
        value.Reset();
    }

    for (auto& value : this->target_views)
    {
        value.Reset();
    }

    for (auto& value : this->so_targets)
    {
        value.Reset();
    }
}

void ff::dx11_device_state::reset(ID3D11DeviceContext* context)
{
    this->clear();
    this->context = context;

    if (context)
    {
        context->IAGetIndexBuffer(&this->ia_index, &this->ia_index_format, &this->ia_index_offset);
        context->IAGetInputLayout(&this->ia_layout);
        context->IAGetPrimitiveTopology(&this->ia_topology);
        context->IAGetVertexBuffers(0, static_cast<UINT>(this->ia_vertexes.size()), this->ia_vertexes[0].GetAddressOf(), this->ia_vertex_strides.data(), this->ia_vertex_offsets.data());

        context->VSGetShader(&this->vs_state.shader, nullptr, nullptr);
        context->VSGetSamplers(0, static_cast<UINT>(this->vs_state.samplers.size()), this->vs_state.samplers[0].GetAddressOf());
        context->VSGetConstantBuffers(0, static_cast<UINT>(this->vs_state.constants.size()), this->vs_state.constants[0].GetAddressOf());
        context->VSGetShaderResources(0, static_cast<UINT>(this->vs_state.resources.size()), this->vs_state.resources[0].GetAddressOf());

        context->GSGetShader(&this->gs_state.shader, nullptr, nullptr);
        context->GSGetSamplers(0, static_cast<UINT>(this->gs_state.samplers.size()), this->gs_state.samplers[0].GetAddressOf());
        context->GSGetConstantBuffers(0, static_cast<UINT>(this->gs_state.constants.size()), this->gs_state.constants[0].GetAddressOf());
        context->GSGetShaderResources(0, static_cast<UINT>(this->gs_state.resources.size()), this->gs_state.resources[0].GetAddressOf());

        context->PSGetShader(&this->ps_state.shader, nullptr, nullptr);
        context->PSGetSamplers(0, static_cast<UINT>(this->ps_state.samplers.size()), this->ps_state.samplers[0].GetAddressOf());
        context->PSGetConstantBuffers(0, static_cast<UINT>(this->ps_state.constants.size()), this->ps_state.constants[0].GetAddressOf());
        context->PSGetShaderResources(0, static_cast<UINT>(this->ps_state.resources.size()), this->ps_state.resources[0].GetAddressOf());

        context->OMGetBlendState(&this->blend, (float*)&this->blend_factor, &this->blend_sample_mask);
        context->OMGetDepthStencilState(&this->depth_state, &this->depth_stencil);
        context->OMGetRenderTargets(static_cast<UINT>(this->target_views.size()), this->target_views[0].GetAddressOf(), &this->depth_view_);
        for (this->target_views_count = 0; this->target_views_count < this->target_views.size() && this->target_views[this->target_views_count]; this->target_views_count++);

        context->SOGetTargets(static_cast<UINT>(this->so_targets.size()), this->so_targets[0].GetAddressOf());
        context->RSGetState(&this->raster);

        UINT count = static_cast<UINT>(this->viewports.size());
        context->RSGetViewports(&count, this->viewports.data());
        this->viewports_count = count;

        count = static_cast<UINT>(this->scissors.size());
        context->RSGetScissorRects(&count, this->scissors.data());
        this->scissor_count = count;
    }
}

void ff::dx11_device_state::apply(dx11_device_state& dest)
{
    assert(!this->context);

    dest.set_vertex_ia(this->ia_vertexes[0].Get(), this->ia_vertex_strides[0], this->ia_vertex_offsets[0]);
    dest.set_index_ia(this->ia_index.Get(), this->ia_index_format, this->ia_index_offset);
    dest.set_layout_ia(this->ia_layout.Get());
    dest.set_topology_ia(this->ia_topology);

    dest.set_vs(this->vs_state.shader.Get());
    dest.set_samplers_vs(this->vs_state.samplers[0].GetAddressOf(), 0, this->vs_state.samplers.size());
    dest.set_constants_vs(this->vs_state.constants[0].GetAddressOf(), 0, this->vs_state.constants.size());
    dest.set_resources_vs(this->vs_state.resources[0].GetAddressOf(), 0, this->vs_state.resources.size());

    dest.set_gs(this->gs_state.shader.Get());
    dest.set_samplers_gs(this->gs_state.samplers[0].GetAddressOf(), 0, this->gs_state.samplers.size());
    dest.set_constants_gs(this->gs_state.constants[0].GetAddressOf(), 0, this->gs_state.constants.size());
    dest.set_resources_gs(this->gs_state.resources[0].GetAddressOf(), 0, this->gs_state.resources.size());

    dest.set_ps(this->ps_state.shader.Get());
    dest.set_samplers_ps(this->ps_state.samplers[0].GetAddressOf(), 0, this->ps_state.samplers.size());
    dest.set_constants_ps(this->ps_state.constants[0].GetAddressOf(), 0, this->ps_state.constants.size());
    dest.set_resources_ps(this->ps_state.resources[0].GetAddressOf(), 0, this->ps_state.resources.size());

    dest.set_blend(this->blend.Get(), this->blend_factor, this->blend_sample_mask);
    dest.set_depth(this->depth_state.Get(), this->depth_stencil);
    dest.set_targets(this->target_views_count ? this->target_views[0].GetAddressOf() : nullptr, this->target_views_count, this->depth_view_.Get());
    dest.set_raster(this->raster.Get());
    dest.set_viewports(this->viewports_count ? this->viewports.data() : nullptr, this->viewports_count);
    dest.set_scissors(this->scissor_count ? this->scissors.data() : nullptr, this->scissor_count);
}

ff::graph_counters ff::dx11_device_state::reset_counters()
{
    ff::graph_counters counters = this->counters;
    this->counters = ff::graph_counters{};
    return counters;
}

void ff::dx11_device_state::draw(size_t count, size_t start)
{
    if (this->context)
    {
        this->context->Draw(static_cast<UINT>(count), static_cast<UINT>(start));
        this->counters.draw++;
    }
}

void ff::dx11_device_state::draw_indexed(size_t index_count, size_t index_start, int vertex_offset)
{
    if (this->context)
    {
        this->context->DrawIndexed(static_cast<UINT>(index_count), static_cast<UINT>(index_start), vertex_offset);
        this->counters.draw++;
    }
}

void* ff::dx11_device_state::map(ID3D11Resource* buffer, D3D11_MAP type, D3D11_MAPPED_SUBRESOURCE* map)
{
    if (this->context && buffer)
    {
        D3D11_MAPPED_SUBRESOURCE stack_map;
        map = map ? map : &stack_map;

        this->context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, map);
        this->counters.map++;

        return map->pData;
    }

    return nullptr;
}

void ff::dx11_device_state::unmap(ID3D11Resource* buffer)
{
    if (this->context && buffer)
    {
        this->context->Unmap(buffer, 0);
    }
}

void ff::dx11_device_state::update_discard(ID3D11Resource* buffer, const void* data, size_t size)
{
    void* dest = size ? this->map(buffer, D3D11_MAP_WRITE_DISCARD) : 0;
    if (dest)
    {
        std::memcpy(dest, data, size);
        this->unmap(buffer);
    }
}

void ff::dx11_device_state::clear_render_target(ID3D11RenderTargetView* view, const DirectX::XMFLOAT4& color)
{
    if (this->context)
    {
        this->context->ClearRenderTargetView(view, &color.x);
        this->counters.clear++;
    }
}

void ff::dx11_device_state::clear_depth_stencil(ID3D11DepthStencilView* view, bool clear_depth, bool clear_stencil, float depth, BYTE stencil)
{
    if (this->context)
    {
        this->context->ClearDepthStencilView(view, (clear_depth ? D3D11_CLEAR_DEPTH : 0) | (clear_stencil ? D3D11_CLEAR_STENCIL : 0), depth, stencil);
        this->counters.depth_clear++;
    }
}

void ff::dx11_device_state::update_subresource(ID3D11Resource* dest, UINT dest_subresource, const D3D11_BOX* dest_box, const void* src_data, UINT src_row_pitch, UINT src_depth_pitch)
{
    if (this->context)
    {
        this->context->UpdateSubresource(dest, dest_subresource, dest_box, src_data, src_row_pitch, src_depth_pitch);
        this->counters.update++;
    }
}

void ff::dx11_device_state::copy_subresource_region(ID3D11Resource* dest_resource, UINT dest_subresource, UINT dest_x, UINT dest_y, UINT dest_z, ID3D11Resource* srcResource, UINT src_subresource, const D3D11_BOX* src_box)
{
    if (this->context)
    {
        this->context->CopySubresourceRegion(dest_resource, dest_subresource, dest_x, dest_y, dest_z, srcResource, src_subresource, src_box);
        this->counters.copy++;
    }
}

void ff::dx11_device_state::set_vertex_ia(ID3D11Buffer* value, size_t stride, size_t offset)
{
    if (this->ia_vertexes[0].Get() != value || this->ia_vertex_strides[0] != stride || this->ia_vertex_offsets[0] != offset)
    {
        this->ia_vertexes[0] = value;
        this->ia_vertex_strides[0] = static_cast<UINT>(stride);
        this->ia_vertex_offsets[0] = static_cast<UINT>(offset);

        if (this->context)
        {
            this->context->IASetVertexBuffers(0, 1, &value, &this->ia_vertex_strides[0], &this->ia_vertex_offsets[0]);
        }
    }
}

void ff::dx11_device_state::set_index_ia(ID3D11Buffer* value, DXGI_FORMAT format, size_t offset)
{
    if (this->ia_index.Get() != value || this->ia_index_format != format || this->ia_index_offset != offset)
    {
        this->ia_index = value;
        this->ia_index_format = format;
        this->ia_index_offset = static_cast<UINT>(offset);

        if (this->context)
        {
            this->context->IASetIndexBuffer(value, this->ia_index_format, this->ia_index_offset);
        }
    }
}

void ff::dx11_device_state::set_layout_ia(ID3D11InputLayout* value)
{
    if (this->ia_layout.Get() != value)
    {
        this->ia_layout = value;

        if (this->context)
        {
            this->context->IASetInputLayout(value);
        }
    }
}

void ff::dx11_device_state::set_topology_ia(D3D_PRIMITIVE_TOPOLOGY value)
{
    if (this->ia_topology != value)
    {
        this->ia_topology = value;

        if (this->context)
        {
            this->context->IASetPrimitiveTopology(value);
        }
    }
}

void ff::dx11_device_state::set_append_so(ID3D11Buffer* value)
{
    if (this->so_targets[0].Get() != value)
    {
        this->set_output_so(value, 0);
    }
}

void ff::dx11_device_state::set_output_so(ID3D11Buffer* value, size_t offset)
{
    this->so_targets[0] = value;

    if (this->context)
    {
        UINT offset2 = static_cast<UINT>(offset);
        this->context->SOSetTargets(1, &value, &offset2);
    }
}

void ff::dx11_device_state::set_vs(ID3D11VertexShader* value)
{
    if (this->vs_state.shader.Get() != value)
    {
        this->vs_state.shader = value;

        if (this->context)
        {
            this->context->VSSetShader(value, nullptr, 0);
        }
    }
}

void ff::dx11_device_state::set_samplers_vs(ID3D11SamplerState* const* values, size_t start, size_t count)
{
    if (std::memcmp(values, this->vs_state.samplers[start].GetAddressOf(), sizeof(ID3D11SamplerState*) * count))
    {
        for (size_t i = start; i < start + count; i++)
        {
            this->vs_state.samplers[i] = values[i - start];
        }

        if (this->context)
        {
            this->context->VSSetSamplers(static_cast<UINT>(start), static_cast<UINT>(count), values);
        }
    }
}

void ff::dx11_device_state::set_constants_vs(ID3D11Buffer* const* values, size_t start, size_t count)
{
    if (std::memcmp(values, this->vs_state.constants[start].GetAddressOf(), sizeof(ID3D11Buffer*) * count))
    {
        for (size_t i = start; i < start + count; i++)
        {
            this->vs_state.constants[i] = values[i - start];
        }

        if (this->context)
        {
            this->context->VSSetConstantBuffers(static_cast<UINT>(start), static_cast<UINT>(count), values);
        }
    }
}

void ff::dx11_device_state::set_resources_vs(ID3D11ShaderResourceView* const* values, size_t start, size_t count)
{
    if (std::memcmp(values, this->vs_state.resources[start].GetAddressOf(), sizeof(ID3D11ShaderResourceView*) * count))
    {
        for (size_t i = start; i < start + count; i++)
        {
            this->vs_state.resources[i] = values[i - start];
        }

        if (this->context)
        {
            this->context->VSSetShaderResources(static_cast<UINT>(start), static_cast<UINT>(count), values);
        }
    }
}

void ff::dx11_device_state::set_gs(ID3D11GeometryShader* value)
{
    if (this->gs_state.shader.Get() != value)
    {
        this->gs_state.shader = value;

        if (this->context)
        {
            this->context->GSSetShader(value, nullptr, 0);
        }
    }
}

void ff::dx11_device_state::set_samplers_gs(ID3D11SamplerState* const* values, size_t start, size_t count)
{
    if (std::memcmp(values, this->gs_state.samplers[start].GetAddressOf(), sizeof(ID3D11SamplerState*) * count))
    {
        for (size_t i = start; i < start + count; i++)
        {
            this->gs_state.samplers[i] = values[i - start];
        }

        if (this->context)
        {
            this->context->GSSetSamplers(static_cast<UINT>(start), static_cast<UINT>(count), values);
        }
    }
}

void ff::dx11_device_state::set_constants_gs(ID3D11Buffer* const* values, size_t start, size_t count)
{
    if (std::memcmp(values, this->gs_state.constants[start].GetAddressOf(), sizeof(ID3D11Buffer*) * count))
    {
        for (size_t i = start; i < start + count; i++)
        {
            this->gs_state.constants[i] = values[i - start];
        }

        if (this->context)
        {
            this->context->GSSetConstantBuffers(static_cast<UINT>(start), static_cast<UINT>(count), values);
        }
    }
}

void ff::dx11_device_state::set_resources_gs(ID3D11ShaderResourceView* const* values, size_t start, size_t count)
{
    if (std::memcmp(values, this->gs_state.resources[start].GetAddressOf(), sizeof(ID3D11ShaderResourceView*) * count))
    {
        for (size_t i = start; i < start + count; i++)
        {
            this->gs_state.resources[i] = values[i - start];
        }

        if (this->context)
        {
            this->context->GSSetShaderResources(static_cast<UINT>(start), static_cast<UINT>(count), values);
        }
    }
}

void ff::dx11_device_state::set_ps(ID3D11PixelShader* value)
{
    if (this->ps_state.shader.Get() != value)
    {
        this->ps_state.shader = value;

        if (this->context)
        {
            this->context->PSSetShader(value, nullptr, 0);
        }
    }
}

void ff::dx11_device_state::set_samplers_ps(ID3D11SamplerState* const* values, size_t start, size_t count)
{
    if (std::memcmp(values, this->ps_state.samplers[start].GetAddressOf(), sizeof(ID3D11SamplerState*) * count))
    {
        for (size_t i = start; i < start + count; i++)
        {
            this->ps_state.samplers[i] = values[i - start];
        }

        if (this->context)
        {
            this->context->PSSetSamplers(static_cast<UINT>(start), static_cast<UINT>(count), values);
        }
    }
}

void ff::dx11_device_state::set_constants_ps(ID3D11Buffer* const* values, size_t start, size_t count)
{
    if (std::memcmp(values, this->ps_state.constants[start].GetAddressOf(), sizeof(ID3D11Buffer*) * count))
    {
        for (size_t i = start; i < start + count; i++)
        {
            this->ps_state.constants[i] = values[i - start];
        }

        if (this->context)
        {
            this->context->PSSetConstantBuffers(static_cast<UINT>(start), static_cast<UINT>(count), values);
        }
    }
}

void ff::dx11_device_state::set_resources_ps(ID3D11ShaderResourceView* const* values, size_t start, size_t count)
{
    if (std::memcmp(values, this->ps_state.resources[start].GetAddressOf(), sizeof(ID3D11ShaderResourceView*) * count))
    {
        for (size_t i = start; i < start + count; i++)
        {
            this->ps_state.resources[i] = values[i - start];
        }

        if (this->context)
        {
            this->context->PSSetShaderResources(static_cast<UINT>(start), static_cast<UINT>(count), values);
        }
    }
}

void ff::dx11_device_state::set_blend(ID3D11BlendState* value, const DirectX::XMFLOAT4& blend_factor, UINT sample_mask)
{
    if (this->blend.Get() != value || this->blend_factor != blend_factor || this->blend_sample_mask != sample_mask)
    {
        this->blend = value;
        this->blend_factor = blend_factor;
        this->blend_sample_mask = static_cast<UINT>(sample_mask);

        if (this->context)
        {
            this->context->OMSetBlendState(value, (const float*)&blend_factor, this->blend_sample_mask);
        }
    }
}

void ff::dx11_device_state::set_depth(ID3D11DepthStencilState* value, UINT stencil)
{
    if (this->depth_state.Get() != value || this->depth_stencil != stencil)
    {
        this->depth_state = value;
        this->depth_stencil = stencil;

        if (this->context)
        {
            this->context->OMSetDepthStencilState(value, stencil);
        }
    }
}

void ff::dx11_device_state::set_targets(ID3D11RenderTargetView* const* targets, size_t count, ID3D11DepthStencilView* depth)
{
    if (this->depth_view_.Get() != depth || this->target_views_count != count ||
        (count && std::memcmp(targets, this->target_views[0].GetAddressOf(), sizeof(ID3D11RenderTargetView*) * count)))
    {
        this->depth_view_ = depth;
        this->target_views_count = count;

        for (size_t i = 0; i < count; i++)
        {
            this->target_views[i] = targets[i];
        }

        for (size_t i = count; i < this->target_views.size(); i++)
        {
            this->target_views[i].Reset();
        }

        if (this->context)
        {
            this->context->OMSetRenderTargets(static_cast<UINT>(count), count ? targets : nullptr, depth);
        }
    }
}

void ff::dx11_device_state::set_raster(ID3D11RasterizerState* value)
{
    if (this->raster.Get() != value)
    {
        this->raster = value;

        if (this->context)
        {
            this->context->RSSetState(value);
        }
    }
}

void ff::dx11_device_state::set_viewports(const D3D11_VIEWPORT* value, size_t count)
{
    if (this->viewports_count != count || (count && std::memcmp(value, this->viewports.data(), sizeof(D3D11_VIEWPORT) * count)))
    {
        this->viewports_count = count;

        if (count)
        {
            std::memcpy(this->viewports.data(), value, sizeof(D3D11_VIEWPORT) * count);
        }

        if (this->context)
        {
            this->context->RSSetViewports(static_cast<UINT>(count), value);
        }
    }
}

void ff::dx11_device_state::set_scissors(const D3D11_RECT* value, size_t count)
{
    if (this->scissor_count != count || (count && std::memcmp(value, this->scissors.data(), sizeof(D3D11_RECT) * count)))
    {
        this->scissor_count = count;

        if (count)
        {
            std::memcpy(this->scissors.data(), value, sizeof(D3D11_RECT) * count);
        }

        if (this->context)
        {
            this->context->RSSetScissorRects(static_cast<UINT>(count), value);
        }
    }
}

const Microsoft::WRL::ComPtr<ID3D11DepthStencilView>& ff::dx11_device_state::depth_view() const
{
    return this->depth_view_;
}
