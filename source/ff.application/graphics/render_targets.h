#pragma once

#include "../dxgi/depth_base.h"
#include "../dxgi/target_base.h"
#include "../dx_types/viewport.h"

namespace ff::dxgi
{
    class palette_base;
}

namespace ff
{
    class render_targets;
    class texture;

    enum class render_target_type
    {
        palette,
        rgba,
        rgba_pma,

        count
    };

    class render_target
    {
    public:
        render_target(ff::point_size size, const DirectX::XMFLOAT4* clear_color = nullptr, int palette_clear_color = 0);
        render_target(render_target&& other) noexcept = default;
        render_target(const render_target& other) = default;

        render_target& operator=(render_target&& other) noexcept = default;
        render_target& operator=(const render_target& other) = default;

    private:
        friend class ff::render_targets;
        static constexpr size_t COUNT = static_cast<size_t>(ff::render_target_type::count);

        ff::point_size size;
        ff::viewport viewport;
        DirectX::XMFLOAT4 clear_color;
        DirectX::XMFLOAT4 palette_clear_color;
        std::shared_ptr<ff::texture> textures[COUNT];
        std::shared_ptr<ff::dxgi::target_base> targets[COUNT];
        bool used_targets[COUNT];
    };

    class render_targets
    {
    public:
        render_targets(const std::shared_ptr<ff::dxgi::target_base>& default_target);
        render_targets(render_targets&& other) noexcept = default;
        render_targets(const render_targets& other) = default;

        render_targets& operator=(render_targets&& other) noexcept = default;
        render_targets& operator=(const render_targets& other) = default;

        void push(ff::render_target& entry);
        ff::rect_float pop(ff::dxgi::command_context_base& context, ff::dxgi::target_base* target, ff::dxgi::palette_base* palette = nullptr);

        const std::shared_ptr<ff::dxgi::target_base>& default_target() const;
        void default_target(const std::shared_ptr<ff::dxgi::target_base>& value);
        ff::dxgi::target_base& target(ff::dxgi::command_context_base& context, ff::render_target_type type = ff::render_target_type::rgba);
        ff::dxgi::depth_base& depth(ff::dxgi::command_context_base& context);

    private:
        std::shared_ptr<ff::dxgi::target_base> default_target_;
        std::shared_ptr<ff::dxgi::depth_base> depth_;
        std::shared_ptr<ff::texture> texture_1080;
        std::shared_ptr<ff::dxgi::target_base> target_1080;
        std::vector<ff::render_target*> entry_stack;
    };
}
