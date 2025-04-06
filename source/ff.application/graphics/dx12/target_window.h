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
        constexpr static size_t BUFFER_COUNT = 2;

        // device_child_base
        virtual void before_reset() override;
        virtual bool reset() override;

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
        ff::stack_vector<ff::dx12::resource, BUFFER_COUNT> target_textures;
        ff::dx12::descriptor_range target_views;
        ff::win_handle latency_handle;

        struct pacing_stage
        {
            uint32_t latency;
            bool vsync;
        };

        struct
        {
            ff::timer timer{};
            double average = ff::constants::seconds_per_update<double>();
            size_t count{};
            size_t stage{};
        } pacing{};

        static std::array<pacing_stage, 4> pacing_stages;
        uint32_t pacing_latency() const;
        bool pacing_vsync() const;
    };
}
