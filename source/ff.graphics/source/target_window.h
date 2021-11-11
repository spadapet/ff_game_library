#pragma once

namespace ff
{
    class target_window : public ff::dxgi::target_window_base, public ff_dx::target_access, private ff::dxgi::device_child_base
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
        virtual bool pre_render(ff::dxgi::command_context_base& context, const DirectX::XMFLOAT4* clear_color) override;
        virtual bool post_render(ff::dxgi::command_context_base& context) override;
        virtual ff::signal_sink<ff::dxgi::target_base*>& render_presented() override;
        virtual ff::dxgi::target_access_base& target_access() override;
        virtual DXGI_FORMAT format() const override;
        virtual ff::window_size size() const override;

        // target_access
#if DXVER == 11
        virtual ID3D11Texture2D* dx11_target_texture() override;
        virtual ID3D11RenderTargetView* dx11_target_view() override;
#elif DXVER == 12
        virtual ID3D12ResourceX* texture() override;
        virtual D3D12_CPU_DESCRIPTOR_HANDLE view() override;
#endif

        // target_window_base
        virtual bool size(const ff::window_size& size) override;
        virtual ff::signal_sink<ff::window_size>& size_changed() override;
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
        ff::signal<ff::dxgi::target_base*> render_presented_;
        ff::signal_connection window_message_connection;
        Microsoft::WRL::ComPtr<IDXGISwapChainX> swap_chain;
#if DXVER == 11
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> view_;
#elif DXVER == 12
        std::array<ff_dx::resource, BACK_BUFFER_COUNT> render_targets;
        std::array<uint64_t, BACK_BUFFER_COUNT> fence_values;
        ff_dx::descriptor_range views;
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
