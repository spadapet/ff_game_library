#pragma once

#include "../dx12/access.h"
#include "../dx12/descriptor_range.h"
#include "../dx12/resource.h"
#include "../dxgi/target_window_base.h"

namespace ff::dx12
{
    class target_window : public ff::dxgi::target_window_base, public ff::dx12::target_access, private ff::dxgi::device_child_base
    {
    public:
        target_window(ff::window* window, const ff::dxgi::target_window_params& params);
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
        virtual size_t frame_latency() const override;
        virtual bool vsync() const override;
        virtual bool allow_full_screen() const override;
        virtual bool full_screen(ff::rect_int* windowed_rect = nullptr) override;
        virtual bool full_screen(bool value, const ff::rect_int* windowed_rect_override = nullptr) override;
        virtual const ff::dxgi::target_window_params& init_params() const override;
        virtual void init_params(const ff::dxgi::target_window_params& params) override;

    private:
        // device_child_base
        virtual void before_reset() override;
        virtual bool reset() override;

        void handle_message(ff::window* window, ff::window_message& msg);
        void before_resize();
        bool internal_reset();
        void handle_latency(ff::dxgi::target_window_params::latency_strategy_t latency_strategy);
        ff::dxgi::target_base& extra_render_target();

        struct
        {
            bool resizing{};
            bool size_valid{};
            ff::window_size size{};
        } resizing_data{}; // main thread

        // Window
        std::mutex window_mutex;
        ff::window* window{}; // main thread, unless window_mutex is locked
        ff::signal_connection window_message_connection; // main thread
        ff::rect_int windowed_rect{}; // main thread
        ff::signal<ff::window_size> size_changed_; // game thread
        ff::window_size cached_size{}; // game thread
        bool was_full_screen_on_close{}; // main thread, unless window_mutex is locked

        // Swap chain (all on game thread)
        Microsoft::WRL::ComPtr<IDXGISwapChain3> swap_chain;
        ff::win_handle frame_latency_handle;
        std::vector<std::unique_ptr<ff::dx12::resource>> target_textures;
        std::vector<std::shared_ptr<ff::dxgi::target_base>> extra_render_targets;
        ff::dx12::descriptor_range target_views;
        ff::dxgi::target_window_params params{};
    };
}
