#pragma once

#include "dx11_render_target_window_base.h"
#include "graphics_child_base.h"

#if !UWP_APP

namespace ff
{
    class dx11_render_target_window
        : public ff::dx11_render_target_window_base
        , public ff::internal::graphics_child_base
    {
    public:
        dx11_render_target_window(ff::window* window);
        dx11_render_target_window(dx11_render_target_window&& other) noexcept = delete;
        dx11_render_target_window(const dx11_render_target_window& other) = delete;
        virtual ~dx11_render_target_window() override;

        dx11_render_target_window& operator=(dx11_render_target_window&& other) noexcept = delete;
        dx11_render_target_window& operator=(const dx11_render_target_window& other) = delete;
        operator bool() const;

        // dx11_render_target_base
        virtual DXGI_FORMAT format() const override;
        virtual ff::window_size size() const override;
        virtual ID3D11Texture2D* texture() override;
        virtual ID3D11RenderTargetView* view() override;

        // dx11_render_target_window_base
        virtual bool present(bool vsync) override;
        virtual bool size(const ff::window_size& size) override;
        virtual ff::signal_sink<void(ff::window_size)>& size_changed() override;
        virtual bool allow_full_screen() const override;
        virtual bool full_screen() override;
        virtual bool full_screen(bool value) override;

        // graphics_child_base
        virtual bool reset() override;

    private:
        void destroy();
        bool set_initial_size();
        void flush_before_resize();
        bool resize_swap_chain(ff::point_int size);

        mutable std::recursive_mutex mutex;
        ff::window* window;
        ff::signal<void(ff::window_size)> size_changed_;
        Microsoft::WRL::ComPtr<IDXGISwapChainX> swap_chain;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> view_;
        bool was_full_screen_on_close;
    };
}

#endif
