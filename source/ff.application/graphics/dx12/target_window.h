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
        target_window(ff::window* window);
        target_window(target_window&& other) noexcept = delete;
        target_window(const target_window& other) = delete;
        virtual ~target_window() override;

        target_window& operator=(target_window&& other) noexcept = delete;
        target_window& operator=(const target_window& other) = delete;
        operator bool() const;

        // target_base
        virtual void clear(ff::dxgi::command_context_base& context, const ff::color& clear_color) override;
        virtual void discard(ff::dxgi::command_context_base& context) override;
        virtual bool begin_render(ff::dxgi::command_context_base& context, const ff::color* clear_color) override;
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
        virtual void notify_window_message(ff::window* window, ff::window_message& message) override;
        virtual size_t buffer_count() const override;

    private:
        // device_child_base
        virtual void before_reset() override;
        virtual bool reset() override;

       
        void handle_latency();
        void update_pacing();
        void before_resize();
        bool internal_reset();

        struct
        {
            bool resizing{};
            bool size_valid{};
            ff::window_size size{};
        } resizing_data{}; // main thread

        // Window
        ff::window* window{}; // main thread
        ff::window_size cached_size{}; // game thread

        // Swap chain (all on game thread)
        Microsoft::WRL::ComPtr<IDXGISwapChain3> swap_chain;
        ff::stack_vector<ff::dx12::resource, 2> target_textures;
        ff::dx12::descriptor_range target_views;
        ff::win_handle latency_handle;

        // Frame pacing
        ff::timer pacing_timer;
        size_t pacing_count{};

        enum class pacing_t
        {
            init,
            normal,
            slow_1, // vsync off, latency 1
            slow_2, // vsync on, latency 2
            slow_3, // vsync off, latency 2
            panic = slow_3,
        } pacing{};

        constexpr static uint32_t pacing_vsync(pacing_t pacing)
        {
            switch (pacing)
            {
                case pacing_t::init:
                case pacing_t::normal:
                case pacing_t::slow_2:
                    return 1;

                default:
                    return 0;
            }
        }

        constexpr static uint32_t pacing_latency(pacing_t pacing)
        {
            switch (pacing)
            {
                case pacing_t::init:
                case pacing_t::normal:
                case pacing_t::slow_1:
                    return 1;

                default:
                    return 2;
            }
        }
    };
}
