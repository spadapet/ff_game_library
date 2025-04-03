#pragma once

#include "../dx12/access.h"
#include "../dx12/descriptor_range.h"
#include "../dxgi/target_base.h"

namespace ff::dxgi
{
    class texture_base;
}

namespace ff::dx12
{
    class texture;

    class target_texture : public ff::dxgi::target_base, public ff::dx12::target_access, private ff::dxgi::device_child_base
    {
    public:
        target_texture(
            const std::shared_ptr<ff::dxgi::texture_base>& texture,
            size_t array_start = 0,
            size_t array_count = 0,
            size_t mip_level = 0,
            int dmdo_rotate = DMDO_DEFAULT,
            double dpi_scale = 0.0);
        target_texture(target_texture&& other) noexcept;
        target_texture(const target_texture& other) = delete;
        virtual ~target_texture() override;

        target_texture& operator=(target_texture&& other) noexcept = default;
        target_texture& operator=(const target_texture& other) = delete;
        operator bool() const;

        const std::shared_ptr<ff::dx12::texture>& shared_texture() const;

        // target_base
        virtual void clear(ff::dxgi::command_context_base& context, const ff::color& clear_color) override;
        virtual void discard(ff::dxgi::command_context_base& context) override;
        virtual bool begin_render(ff::dxgi::command_context_base& context, const ff::color* clear_color) override;
        virtual bool end_render(ff::dxgi::command_context_base& context) override;
        virtual ff::dxgi::target_access_base& target_access() override;
        virtual size_t target_array_start() const override;
        virtual size_t target_array_size() const override;
        virtual size_t target_mip_start() const override;
        virtual size_t target_mip_size() const override;
        virtual size_t target_sample_count() const override;
        virtual DXGI_FORMAT format() const override;
        virtual ff::window_size size() const override;

        // target_access
        virtual ff::dx12::resource& dx12_target_texture() override;
        virtual D3D12_CPU_DESCRIPTOR_HANDLE dx12_target_view() override;

    private:
        // device_child_base
        virtual bool reset() override;

        std::shared_ptr<ff::dx12::texture> texture_;
        ff::dx12::descriptor_range view_;
        size_t array_start;
        size_t array_count;
        size_t mip_level;
        int dmdo_rotate;
        double dpi_scale;
    };
}
