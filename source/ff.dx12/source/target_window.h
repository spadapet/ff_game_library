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
        target_window(ff::window* window, size_t buffer_count, size_t frame_latency, bool vsync, bool allow_full_screen);
        target_window(target_window&& other) noexcept = delete;
        target_window(const target_window& other) = delete;
        virtual ~target_window() override;

        target_window& operator=(target_window&& other) noexcept = delete;
        target_window& operator=(const target_window& other) = delete;
        operator bool() const;

        // target_base
        virtual void clear(ff::dxgi::command_context_base& context, const DirectX::XMFLOAT4& clear_color) override;
        virtual bool begin_render(ff::dxgi::command_context_base& context, const DirectX::XMFLOAT4* clear_color) override;
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

        // target_window_base
        virtual bool size(const ff::window_size& size) override;
        virtual ff::signal_sink<ff::window_size>& size_changed() override;
        virtual size_t buffer_count() const override;
        virtual void buffer_count(size_t value) override;
        virtual size_t frame_latency() const override;
        virtual void frame_latency(size_t value) override;
        virtual bool vsync() const override;
        virtual void vsync(bool value) override;
        virtual void wait_for_render_ready() override;
        virtual bool allow_full_screen() const override;
        virtual bool full_screen() override;
        virtual bool full_screen(bool value) override;

    private:
        // device_child_base
        virtual void before_reset() override;
        virtual bool reset() override;

        void handle_message(ff::window_message& msg);
        void before_resize();
        bool internal_reset(const ff::window_size& size, size_t buffer_count, size_t frame_latency);
        bool internal_size(const ff::window_size& size, size_t buffer_count, size_t frame_latency);

        ff::window* window{};
        ff::window_size cached_size{};
        ff::signal<ff::window_size> size_changed_;
        ff::signal_connection window_message_connection;
        ff::win_handle frame_latency_handle;
        ff::win_event target_ready_event;
        Microsoft::WRL::ComPtr<IDXGISwapChain3> swap_chain;

        std::vector<std::unique_ptr<ff::dx12::resource>> target_textures2;
        std::vector<ff::dx12::fence_value> target_fence_values2;
        ff::dx12::descriptor_range target_views2;
        size_t back_buffer_index{};

        bool main_window{};
        bool allow_full_screen_{};
        bool was_full_screen_on_close{};
        bool vsync_{ true };
#if UWP_APP
        bool use_xaml_composition{};
        bool cached_full_screen_uwp{};
        bool full_screen_uwp{};
#endif
    };
}
