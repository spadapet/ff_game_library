#include "pch.h"
#include "commands.h"
#include "descriptor_allocator.h"
#include "draw_device.h"
#include "globals.h"
#include "gpu_event.h"
#include "resource.h"
#include "texture.h"

static void get_texture_sampler_desc(D3D12_FILTER filter, D3D12_SAMPLER_DESC& desc)
{
    desc = {};
    desc.Filter = filter;
    desc.AddressU = desc.AddressV = desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    desc.MaxLOD = D3D12_FLOAT32_MAX;
}

static D3D12_VIEWPORT get_viewport(const ff::rect_float& view_rect)
{
    D3D12_VIEWPORT viewport;
    viewport.TopLeftX = view_rect.left;
    viewport.TopLeftY = view_rect.top;
    viewport.Width = view_rect.width();
    viewport.Height = view_rect.height();
    viewport.MinDepth = 0;
    viewport.MaxDepth = 1;

    return viewport;
}

ff::internal::dx12::draw_device_base::draw_device_base()
    : samplers_gpu(ff::dx12::gpu_sampler_descriptors().alloc_pinned_range(2))
{}

bool ff::internal::dx12::draw_device_base::valid() const
{
    return this->internal_valid();
}

void ff::internal::dx12::draw_device_base::internal_destroy()
{
    assert(!this->commands);
}

void ff::internal::dx12::draw_device_base::internal_reset()
{
    // Create samplers
    D3D12_SAMPLER_DESC point_desc, linear_desc;
    ::get_texture_sampler_desc(D3D12_FILTER_MIN_MAG_MIP_POINT, point_desc);
    ::get_texture_sampler_desc(D3D12_FILTER_MIN_MAG_MIP_LINEAR, linear_desc);

    ff::dx12::descriptor_range samplers_cpu = ff::dx12::cpu_sampler_descriptors().alloc_range(2);
    ff::dx12::device()->CreateSampler(&point_desc, samplers_cpu.cpu_handle(0));
    ff::dx12::device()->CreateSampler(&linear_desc, samplers_cpu.cpu_handle(1));
    ff::dx12::device()->CopyDescriptorsSimple(2, this->samplers_gpu.cpu_handle(0), samplers_cpu.cpu_handle(0), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
}

ff::dxgi::command_context_base* ff::internal::dx12::draw_device_base::internal_flush(ff::dxgi::command_context_base* context, bool end_draw)
{
    if (end_draw)
    {
        this->commands->end_event();
        this->commands = nullptr;
    }

    return this->commands;
}

ff::dxgi::command_context_base* ff::internal::dx12::draw_device_base::internal_setup(
    ff::dxgi::command_context_base& context,
    ff::dxgi::target_base& target,
    ff::dxgi::depth_base* depth,
    const ff::rect_float& view_rect,
    bool ignore_rotation)
{
    ff::dx12::commands& commands = ff::dx12::commands::get(context);

    if (depth)
    {
        assert_ret_val(depth->physical_size(commands, target.size().physical_pixel_size()), nullptr);
        depth->clear(commands, 0, 0);
    }

    const ff::rect_float physical_view_rect = !ignore_rotation
        ? target.size().logical_to_physical_rect(view_rect)
        : view_rect;

    this->setup_target = &target;
    this->setup_viewport = ::get_viewport(physical_view_rect);
    this->setup_depth = depth;

    this->commands = &commands;
    this->commands->begin_event(ff::dx12::gpu_event::draw_2d);
    this->commands->targets(&this->setup_target, 1, this->setup_depth);
    this->commands->viewports(&this->setup_viewport, 1);
    this->commands->scissors(nullptr, 1);

    return this->commands;
}

void ff::internal::dx12::draw_device_base::internal_flush_begin(ff::dxgi::command_context_base* context)
{
    this->commands->begin_event(ff::dx12::gpu_event::draw_batch);
}

void ff::internal::dx12::draw_device_base::internal_flush_end(ff::dxgi::command_context_base* context)
{
    this->commands->end_event();
}

void ff::internal::dx12::draw_device_base::update_palette_texture(ff::dxgi::command_context_base& context,
    size_t textures_using_palette_count,
    ff::dxgi::texture_base& palette_texture, size_t* palette_texture_hashes, palette_to_index_t& palette_to_index,
    ff::dxgi::texture_base& palette_remap_texture, size_t* palette_remap_texture_hashes, palette_remap_to_index_t& palette_remap_to_index)
{
    this->commands->begin_event(ff::dx12::gpu_event::update_palette);

    if (textures_using_palette_count && !palette_to_index.empty())
    {
        ff::dx12::texture& dest_texture = ff::dx12::texture::get(palette_texture);

        for (const auto& iter : palette_to_index)
        {
            ff::dxgi::palette_base* palette = iter.second.first;
            if (palette)
            {
                unsigned int index = iter.second.second;
                size_t palette_row = palette->current_row();
                const ff::dxgi::palette_data_base* palette_data = palette->data();
                size_t row_hash = palette_data->row_hash(palette_row);

                if (palette_texture_hashes[index] != row_hash)
                {
                    palette_texture_hashes[index] = row_hash;
                    ff::dx12::texture& src_texture = ff::dx12::texture::get(*palette_data->texture());
                    this->commands->copy_texture(
                        *dest_texture.dx12_resource_updated(*this->commands), 0, ff::point_size(0, index),
                        *src_texture.dx12_resource_updated(*this->commands), 0, ff::rect_size(0, palette_row, ff::dxgi::palette_size, palette_row + 1));
                }
            }
        }
    }

    if ((textures_using_palette_count || this->target_requires_palette()) && !palette_remap_to_index.empty())
    {
        ff::dx12::texture& dest_remap_texture = ff::dx12::texture::get(palette_remap_texture);
        DXGI_FORMAT dest_format = dest_remap_texture.dx12_resource_updated(*this->commands)->desc().Format;
        DirectX::Image remap_image{ ff::dxgi::palette_size, 1, dest_format, ff::dxgi::palette_size, ff::dxgi::palette_size };

        for (const auto& iter : palette_remap_to_index)
        {
            unsigned int row = iter.second.second;
            size_t row_hash = iter.first;

            if (palette_remap_texture_hashes[row] != row_hash)
            {
                palette_remap_texture_hashes[row] = row_hash;
                remap_image.pixels = const_cast<uint8_t*>(iter.second.first);
                dest_remap_texture.update(*this->commands, 0, 0, ff::point_size(0, row), remap_image);
            }
        }
    }

    this->commands->end_event();
}

std::unique_ptr<ff::dxgi::draw_device_base> ff::dx12::create_draw_device()
{
    return ff::dx12::supports_mesh_shaders()
        ? ff::internal::dx12::create_draw_device_ms()
        : ff::internal::dx12::create_draw_device_gs();
}
