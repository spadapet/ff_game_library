#pragma once

#include "draw_base.h"
#include "draw_ptr.h"

namespace ff
{
    class dx11_draw
    {
    public:
        dx11_draw();
        dx11_draw(dx11_draw&& other) noexcept = delete;
        dx11_draw(const dx11_draw& other) = delete;

        dx11_draw& operator=(dx11_draw&& other) noexcept = delete;
        dx11_draw& operator=(const dx11_draw& other) = delete;
        operator bool() const;

        // ff::draw_ptr draw_begin(IRenderTarget* target, IRenderDepth* depth, RectFloat viewRect, RectFloat worldRect, RendererOptions options = RendererOptions::None) = 0;
        // ff::draw_ptr draw_begin(IRenderTarget* target, IRenderDepth* depth, RectFixedInt viewRect, RectFixedInt worldRect, RendererOptions options = (RendererOptions)0);
    };
}
