#pragma once

namespace ff::internal::dx12
{
    class draw_device_base : public ff::dxgi::draw_util::draw_device_base, public ff::dxgi::draw_device_base
    {
    public:
        draw_device_base();

        virtual bool valid() const override;

    protected:
        virtual void internal_destroy() override;
        virtual void internal_reset() override;
        virtual ff::dxgi::command_context_base* internal_flush(ff::dxgi::command_context_base* context, bool end_draw) override;

        virtual ff::dxgi::command_context_base* internal_setup(
            ff::dxgi::command_context_base& context,
            ff::dxgi::target_base& target,
            ff::dxgi::depth_base* depth,
            const ff::rect_float& view_rect,
            bool ignore_rotation) override;

        virtual void internal_flush_begin(ff::dxgi::command_context_base* context) override;
        virtual void internal_flush_end(ff::dxgi::command_context_base* context) override;

        virtual void update_palette_texture(ff::dxgi::command_context_base& context,
            size_t textures_using_palette_count,
            ff::dxgi::texture_base& palette_texture, size_t* palette_texture_hashes, palette_to_index_t& palette_to_index,
            ff::dxgi::texture_base& palette_remap_texture, size_t* palette_remap_texture_hashes, palette_remap_to_index_t& palette_remap_to_index) override;


    protected:
        ff::dx12::descriptor_range samplers_gpu; // 0:point, 1:linear
        ff::dx12::commands* commands{};
        ff::dxgi::target_base* setup_target{};
        ff::dxgi::depth_base* setup_depth{};
        D3D12_VIEWPORT setup_viewport{};
    };

    std::unique_ptr<ff::dxgi::draw_device_base> create_draw_device_gs();
    std::unique_ptr<ff::dxgi::draw_device_base> create_draw_device_ms();
}

namespace ff::dx12
{
    std::unique_ptr<ff::dxgi::draw_device_base> create_draw_device();
}
