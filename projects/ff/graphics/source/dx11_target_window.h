#pragma once

#include "target_base.h"
#include "target_window_base.h"
#include "graphics_child_base.h"

namespace ff
{
    class dx11_target_window
        : public ff::target_window_base
        , public ff::internal::graphics_child_base
    {
    public:
        dx11_target_window();
        dx11_target_window(ff::window* window);
        dx11_target_window(dx11_target_window&& other) noexcept = delete;
        dx11_target_window(const dx11_target_window& other) = delete;
        virtual ~dx11_target_window() override;

        dx11_target_window& operator=(dx11_target_window&& other) noexcept = delete;
        dx11_target_window& operator=(const dx11_target_window& other) = delete;
        operator bool() const;

        // target_base
        virtual DXGI_FORMAT format() const override;
        virtual ff::window_size size() const override;
        virtual ID3D11Texture2D* texture() override;
        virtual ID3D11RenderTargetView* view() override;

        // target_window_base
        virtual bool present(bool vsync) override;
        virtual bool size(const ff::window_size& size) override;
        virtual ff::signal_sink<ff::window_size>& size_changed() override;
        virtual bool allow_full_screen() const override;
        virtual bool full_screen() override;
        virtual bool full_screen(bool value) override;

        // graphics_child_base
        virtual bool reset() override;
        virtual int reset_priority() const override;

    private:
        void handle_message(ff::window_message& msg);

        ff::window* window;
        ff::window_size cached_size;
        ff::signal<ff::window_size> size_changed_;
        ff::signal_connection window_message_connection;
        Microsoft::WRL::ComPtr<IDXGISwapChainX> swap_chain;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> view_;
        bool main_window;
        bool was_full_screen_on_close;
#if UWP_APP
        bool use_xaml_composition;
        bool cached_full_screen_uwp;
        bool full_screen_uwp;
#endif
    };
}
