#pragma once

#include "descriptor_range.h"
#include "fence_value.h"
#include "resource.h"
#include "target_access.h"

namespace ff::dx12
{
    class target_window : public ff::dxgi::target_window_base, public ff::dx12::target_access, private ff::dxgi::device_child_base
    {
    public:
        target_window();
        target_window(ff::window* window);
        target_window(target_window&& other) noexcept = delete;
        target_window(const target_window& other) = delete;
        virtual ~target_window() override;

        target_window& operator=(target_window&& other) noexcept = delete;
        target_window& operator=(const target_window& other) = delete;
        operator bool() const;

        // target_base
        virtual void clear(ff::dxgi::command_context_base& context, const DirectX::XMFLOAT4& clear_color) override;
        virtual bool begin_render(const DirectX::XMFLOAT4* clear_color) override;
        virtual bool end_render() override;
        virtual ff::dxgi::target_access_base& target_access() override;
        virtual size_t target_array_start() const override;
        virtual size_t target_array_size() const override;
        virtual size_t target_mip_start() const override;
        virtual size_t target_mip_size() const override;
        virtual DXGI_FORMAT format() const override;
        virtual ff::window_size size() const override;

        // target_access
        virtual ff::dx12::resource& dx12_target_texture() override;
        virtual D3D12_CPU_DESCRIPTOR_HANDLE dx12_target_view() override;

        // target_window_base
        virtual bool size(const ff::window_size& size) override;
        virtual ff::signal_sink<ff::window_size>& size_changed() override;
        virtual void wait_for_render_ready() override;
        virtual bool allow_full_screen() const override;
        virtual bool full_screen() override;
        virtual bool full_screen(bool value) override;

    private:
        // device_child_base
        virtual bool reset() override;

        static const size_t BACK_BUFFER_COUNT = 2;

        void handle_message(ff::window_message& msg);
        void before_resize();
        void internal_reset();

        ff::window* window;
        ff::window_size cached_size;
        ff::signal<ff::window_size> size_changed_;
        ff::signal_connection window_message_connection;
        ff::win_handle frame_latency_handle;
        ff::win_handle target_ready_event;
        Microsoft::WRL::ComPtr<IDXGISwapChainX> swap_chain;

        std::array<std::unique_ptr<ff::dx12::resource>, BACK_BUFFER_COUNT> target_textures;
        std::array<ff::dx12::fence_value, BACK_BUFFER_COUNT> target_fence_values;
        ff::dx12::descriptor_range target_views;
        size_t back_buffer_index;

        bool main_window;
        bool was_full_screen_on_close;
#if UWP_APP
        bool use_xaml_composition;
        bool cached_full_screen_uwp;
        bool full_screen_uwp;
#endif
    };
}
