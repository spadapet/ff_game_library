#pragma once

#include "target_base.h"
#include "target_window_base.h"
#include "graphics_child_base.h"

namespace ff
{
    class target_window
        : public ff::target_window_base
        , public ff::internal::graphics_child_base
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
        virtual DXGI_FORMAT format() const override;
        virtual ff::window_size size() const override;
#if DXVER == 11
        virtual ID3D11Texture2D* texture() override;
        virtual ID3D11RenderTargetView* view() override;
#elif DXVER == 12
        virtual D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle() override;
        virtual ID3D12ResourceX* rtv_resource() override;
        virtual ID3D12CommandAllocatorX* command_allocator() override;
        virtual ID3D12GraphicsCommandListX* command_list() override;
#endif

        // target_window_base
        virtual void prerender() override;
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
        static const size_t BACK_BUFFER_COUNT = 2;

        void handle_message(ff::window_message& msg);
        void before_resize();
        void internal_reset();
#if DXVER == 12
        void wait_for_gpu();
        void internal_reset(bool for_resize);
#endif

        ff::window* window;
        ff::window_size cached_size;
        ff::signal<ff::window_size> size_changed_;
        ff::signal_connection window_message_connection;
        ff::win_handle swap_chain_latency_handle;
        Microsoft::WRL::ComPtr<IDXGISwapChainX> swap_chain;
#if DXVER == 11
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> view_;
#elif DXVER == 12
        std::array<Microsoft::WRL::ComPtr<ID3D12CommandAllocatorX>, BACK_BUFFER_COUNT> command_allocators;
        std::array<Microsoft::WRL::ComPtr<ID3D12ResourceX>, BACK_BUFFER_COUNT> render_targets;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandListX> command_list_;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeapX> rtv_desc_heap;
        Microsoft::WRL::ComPtr<ID3D12FenceX> fence;
        std::array<UINT64, BACK_BUFFER_COUNT> fence_values;
        ff::win_handle fence_event;
        size_t rtv_desc_size;
        size_t back_buffer_index;
#endif
        bool main_window;
        bool was_full_screen_on_close;
#if UWP_APP
        bool use_xaml_composition;
        bool cached_full_screen_uwp;
        bool full_screen_uwp;
#endif
    };
}
