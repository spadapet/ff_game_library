#pragma once

#include "../dxgi/depth_base.h"
#include "../dxgi/target_base.h"
#include "../resource/texture_resource.h"
#include "../types/color.h"

namespace ff
{
    class render_targets
    {
    public:
        render_targets(size_t count, ff::point_size size, double dpi_scale, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM, size_t sample_count = 1, const ff::color* optimized_clear_color = nullptr);
        render_targets(render_targets&& other) noexcept = default;
        render_targets(const render_targets& other) = delete;

        render_targets& operator=(render_targets&& other) noexcept = default;
        render_targets& operator=(const render_targets& other) = delete;

        size_t count() const;
        void count(size_t value);

        ff::point_size size() const;
        double dpi_scale() const;
        void size(ff::point_size value, double dpi_scale = 1.0);

        DXGI_FORMAT format() const;
        size_t sample_count() const;
        const ff::color& clear_color() const;

        void clear(ff::dxgi::command_context_base& context, size_t index);
        void discard(ff::dxgi::command_context_base& context, size_t index);
        ff::texture& texture(size_t index);
        ff::dxgi::target_base& target(size_t index);
        ff::dxgi::depth_base& depth(size_t index);

    private:
        struct target_t
        {
            std::unique_ptr<ff::texture> texture;
            std::shared_ptr<ff::dxgi::target_base> target;
            std::shared_ptr<ff::dxgi::depth_base> depth;
        };

        DXGI_FORMAT format_;
        size_t sample_count_;
        double dpi_scale_;
        ff::point_size size_;
        ff::color clear_color_;
        std::vector<ff::render_targets::target_t> targets;
    };
}
