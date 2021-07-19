#include "pch.h"
#include "dx11_device_state.h"
#include "dxgi_util.h"
#include "graphics.h"
#include "target_window.h"
#include "texture_util.h"

ff::target_window::target_window()
    : target_window(ff::window::main())
{}

ff::target_window::target_window(ff::window* window)
    : window(window)
    , cached_size{}
    , main_window(ff::window::main() == window)
    , window_message_connection(window->message_sink().connect(std::bind(&target_window::handle_message, this, std::placeholders::_1)))
    , was_full_screen_on_close(false)
#if UWP_APP
    , use_xaml_composition(false)
    , cached_full_screen_uwp(false)
    , full_screen_uwp(false)
#endif
#if DXVER == 12
    , rtv_desc_size(0)
    , back_buffer_index(0)
    , fence_values{}
    , fence_event(ff::create_event())
#endif
{
    this->size(this->window->size());

    ff::internal::graphics::add_child(this);

    if (this->main_window)
    {
        ff::graphics::defer::set_full_screen_target(this);
    }
}

ff::target_window::~target_window()
{
    if (this->main_window &&this->swap_chain)
    {
        this->swap_chain->SetFullscreenState(FALSE, nullptr);
    }

    ff::graphics::defer::remove_target(this);
    ff::internal::graphics::remove_child(this);
}

ff::target_window::operator bool() const
{
    return this->swap_chain && this->window;
}

DXGI_FORMAT ff::target_window::format() const
{
    return DXGI_FORMAT_B8G8R8A8_UNORM;
}

ff::window_size ff::target_window::size() const
{
    return this->cached_size;
}

#if DXVER == 11

ID3D11Texture2D* ff::target_window::texture()
{
    return this->texture_.Get();
}

ID3D11RenderTargetView* ff::target_window::view()
{
    return this->view_.Get();
}

bool ff::target_window::present(bool vsync)
{
    ff::graphics::dx11_device_state().set_targets(nullptr, 0, nullptr);

    HRESULT hr = E_FAIL;
    if (*this)
    {
        DXGI_PRESENT_PARAMETERS pp{};
        hr = this->swap_chain->Present1(vsync ? 1 : 0, 0, &pp);
        ff::graphics::dx11_device_context()->DiscardView1(this->view_.Get(), nullptr, 0);
    }

    return hr != DXGI_ERROR_DEVICE_RESET && hr != DXGI_ERROR_DEVICE_REMOVED;
}

void ff::target_window::before_resize()
{
    this->view_.Reset();
    this->texture_.Reset();
}

void ff::target_window::internal_reset()
{
    this->swap_chain.Reset();
    this->view_.Reset();
    this->texture_.Reset();

    ff::graphics::dx11_device_state().clear();
}

#elif DXVER == 12

D3D12_CPU_DESCRIPTOR_HANDLE ff::target_window::rtv_handle()
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = this->rtv_desc_heap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += this->back_buffer_index * this->rtv_desc_size;
    return handle;
}

ID3D12ResourceX* ff::target_window::rtv_resource()
{
    return this->render_targets[this->back_buffer_index].Get();
}

bool ff::target_window::present(bool vsync)
{
    if (*this)
    {
        DXGI_PRESENT_PARAMETERS pp{};
        HRESULT hr = this->swap_chain->Present1(vsync ? 1 : 0, 0, &pp);

        if (hr != DXGI_ERROR_DEVICE_RESET && hr != DXGI_ERROR_DEVICE_REMOVED)
        {
            //ff::wait_for_handle(this->swap_chain->GetFrameLatencyWaitableObject());
            this->wait_for_gpu();
            return true;
        }
    }

    return false;
}

void ff::target_window::before_resize()
{
    this->internal_reset(true);
}

void ff::target_window::internal_reset()
{
    this->internal_reset(false);
}

void ff::target_window::internal_reset(bool for_resize)
{
    this->wait_for_gpu();

    this->rtv_desc_heap.Reset();
    this->rtv_desc_size = 0;

    if (!for_resize)
    {
        this->fence.Reset();
    }

    for (size_t i = 0; i < ff::target_window::BACK_BUFFER_COUNT; i++)
    {
        if (!for_resize)
        {
            this->fence_values[i] = this->fence_values[this->back_buffer_index];
            this->command_allocators[i].Reset();
        }

        this->render_targets[i].Reset();
    }
}

void ff::target_window::wait_for_gpu()
{
    const UINT64 current_fence_value = this->fence_values[this->back_buffer_index];

    if (SUCCEEDED(ff::graphics::dx12_command_queue()->Signal(this->fence.Get(), current_fence_value)))
    {
        this->back_buffer_index = static_cast<size_t>(this->swap_chain->GetCurrentBackBufferIndex());

        // Check to see if the next frame is ready to start.
        if (this->fence->GetCompletedValue() < this->fence_values[this->back_buffer_index] &&
            SUCCEEDED(this->fence->SetEventOnCompletion(this->fence_values[this->back_buffer_index], this->fence_event)))
        {
            ff::wait_for_event_and_reset(this->fence_event);
        }

        this->fence_values[this->back_buffer_index] = current_fence_value + 1;
    }
}

#endif

bool ff::target_window::size(const ff::window_size& size)
{
    ff::window_size old_size = this->cached_size;
    ff::point_t<UINT> buffer_size = size.rotated_pixel_size().cast<UINT>();
    this->cached_size = size;
#if UWP_APP
    this->cached_full_screen_uwp = false;
#endif

    if (this->swap_chain && old_size != size) // on game thread
    {
        this->before_resize();

        DXGI_SWAP_CHAIN_DESC1 desc;
        this->swap_chain->GetDesc1(&desc);
        if (FAILED(this->swap_chain->ResizeBuffers(ff::target_window::BACK_BUFFER_COUNT, buffer_size.x, buffer_size.y, desc.Format, desc.Flags)))
        {
            assert(false);
            return false;
        }
    }
    else if (!this->swap_chain) // first init on UI thread, reset is on game thread
    {
        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Width = buffer_size.x;
        desc.Height = buffer_size.y;
        desc.Format = this->format();
        desc.SampleDesc.Count = 1;
        desc.BufferCount = ff::target_window::BACK_BUFFER_COUNT;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.Scaling = DXGI_SCALING_STRETCH;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        // desc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

        Microsoft::WRL::ComPtr<IDXGISwapChain1> new_swap_chain;
        Microsoft::WRL::ComPtr<IDXGIFactoryX> factory = ff::graphics::dxgi_factory_for_device();
#if DXVER == 11
        Microsoft::WRL::ComPtr<ID3D11DeviceX> device = ff::graphics::dx11_device();
#elif DXVER == 12
        Microsoft::WRL::ComPtr<ID3D12CommandQueueX> device = ff::graphics::dx12_command_queue();
#endif

        ff::thread_dispatch::get_main()->send([this, factory, device, &new_swap_chain, &desc]()
            {
#if UWP_APP
                if (this->window)
                {
                    Windows::UI::Xaml::Controls::SwapChainPanel^ swap_chain_panel = this->window->swap_chain_panel();
                    this->use_xaml_composition = (swap_chain_panel != nullptr);

                    if (this->use_xaml_composition)
                    {
                        Microsoft::WRL::ComPtr<ISwapChainPanelNative> native_panel;

                        if (FAILED(reinterpret_cast<IUnknown*>(swap_chain_panel)->QueryInterface(__uuidof(ISwapChainPanelNative), reinterpret_cast<void**>(native_panel.GetAddressOf()))) ||
                            FAILED(factory->CreateSwapChainForComposition(device.Get(), &desc, nullptr, &new_swap_chain)) ||
                            FAILED(native_panel->SetSwapChain(new_swap_chain.Get())))
                        {
                            assert(false);
                        }
                    }
                    else if (FAILED(factory->CreateSwapChainForCoreWindow(device.Get(), reinterpret_cast<IUnknown*>(this->window->handle()), &desc, nullptr, &new_swap_chain)))
                    {
                        assert(false);
                    }
                }
#else
                if (!*this->window ||
                    FAILED(factory->CreateSwapChainForHwnd(device.Get(), *this->window, &desc, nullptr, nullptr, &new_swap_chain)) ||
                    FAILED(factory->MakeWindowAssociation(*this->window, DXGI_MWA_NO_WINDOW_CHANGES)))
                {
                    assert(false);
                }
#endif
            });

        if (!new_swap_chain || FAILED(new_swap_chain.As(&this->swap_chain)))
        {
            assert(false);
            return false;
        }
    }

    DXGI_MODE_ROTATION display_rotation = ff::internal::get_display_rotation(
        ff::internal::get_dxgi_rotation(size.native_rotation),
        ff::internal::get_dxgi_rotation(size.current_rotation));

#if UWP_APP
    // Scale the back buffer to the panel
    DXGI_MATRIX_3X2_F inverse_scale{};
    inverse_scale._11 = inverse_scale._22 = 1 / static_cast<float>(size.dpi_scale);
#endif

    if (!this->swap_chain ||
#if UWP_APP
        (this->use_xaml_composition && FAILED(this->swap_chain->SetMatrixTransform(&inverse_scale))) ||
#endif
        FAILED(this->swap_chain->SetRotation(display_rotation)))
    {
        assert(false);
        return false;
    }

#if DXVER == 11
    if ((!this->texture_ && FAILED(this->swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(this->texture_.GetAddressOf())))) ||
        (!this->view_ && (this->view_ = ff::internal::create_target_view(this->texture_.Get())) == nullptr))
    {
        assert(false);
        return false;
    }
#elif DXVER == 12
    this->back_buffer_index = static_cast<UINT>(this->swap_chain->GetCurrentBackBufferIndex());

    if (!this->fence)
    {
        if (FAILED(ff::graphics::dx12_device()->CreateFence(this->fence_values[this->back_buffer_index], D3D12_FENCE_FLAG_NONE,
            __uuidof(ID3D12FenceX), reinterpret_cast<void**>(this->fence.GetAddressOf()))))
        {
            assert(false);
            return false;
        }

        this->fence_values[this->back_buffer_index]++;
    }

    if (!this->rtv_desc_heap)
    {
        D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc{};
        rtv_heap_desc.NumDescriptors = static_cast<UINT>(ff::target_window::BACK_BUFFER_COUNT);
        rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

        if (FAILED(ff::graphics::dx12_device()->CreateDescriptorHeap(&rtv_heap_desc, __uuidof(ID3D12DescriptorHeapX), reinterpret_cast<void**>(this->rtv_desc_heap.GetAddressOf()))) ||
            !(this->rtv_desc_size = ff::graphics::dx12_device()->GetDescriptorHandleIncrementSize(rtv_heap_desc.Type)))
        {
            assert(false);
            return false;
        }
    }

    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = this->rtv_desc_heap->GetCPUDescriptorHandleForHeapStart();

    for (size_t i = 0; i < ff::target_window::BACK_BUFFER_COUNT; i++)
    {
        if ((!this->command_allocators[i] && FAILED(ff::graphics::dx12_device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                __uuidof(ID3D12CommandAllocatorX), reinterpret_cast<void**>(this->command_allocators[i].GetAddressOf())))) ||
            (!this->render_targets[i] && FAILED(this->swap_chain->GetBuffer(static_cast<UINT>(i),
                __uuidof(ID3D12ResourceX), reinterpret_cast<void**>(this->render_targets[i].GetAddressOf())))))
        {
            assert(false);
            return false;
        }

        ff::graphics::dx12_device()->CreateRenderTargetView(this->render_targets[i].Get(), nullptr, rtv_handle);
        rtv_handle.ptr += this->rtv_desc_size;
    }
#endif

    this->size_changed_.notify(size);

    return true;
}

ff::signal_sink<ff::window_size>& ff::target_window::size_changed()
{
    return this->size_changed_;
}

bool ff::target_window::allow_full_screen() const
{
    return this->main_window;
}

bool ff::target_window::full_screen()
{
    if (this->main_window && *this)
    {
#if UWP_APP
        if (!this->cached_full_screen_uwp)
        {
            this->full_screen_uwp = this->window->application_view()->IsFullScreenMode;
            this->cached_full_screen_uwp = true;
        }

        return this->full_screen_uwp;
#else
        BOOL full_screen = FALSE;
        Microsoft::WRL::ComPtr<IDXGIOutput> output;
        return SUCCEEDED(this->swap_chain->GetFullscreenState(&full_screen, &output)) && full_screen;
#endif
    }

    return this->was_full_screen_on_close;
}

bool ff::target_window::full_screen(bool value)
{
    if (this->main_window && *this && !value != !this->full_screen())
    {
#if UWP_APP
        if (value)
        {
            return this->window->application_view()->TryEnterFullScreenMode();
        }
        else
        {
            this->window->application_view()->ExitFullScreenMode();
            return true;
        }
#else
        if (SUCCEEDED(this->swap_chain->SetFullscreenState(value, nullptr)))
        {
            return this->size(this->window->size());
        }
#endif
    }

    return false;
}

bool ff::target_window::reset()
{
    BOOL full_screen = FALSE;
    if (this->main_window && this->swap_chain)
    {
        Microsoft::WRL::ComPtr<IDXGIOutput> output;
        this->swap_chain->GetFullscreenState(&full_screen, &output);
        this->swap_chain->SetFullscreenState(FALSE, nullptr);
    }

    this->internal_reset();

    if (!this->window || !this->size(this->window->size()))
    {
        assert(false);
        return false;
    }

    if (this->main_window && this->swap_chain && full_screen)
    {
        this->swap_chain->SetFullscreenState(TRUE, nullptr);
    }

    return *this;
}

int ff::target_window::reset_priority() const
{
    return -100;
}

void ff::target_window::handle_message(ff::window_message& msg)
{
    switch (msg.msg)
    {
        case WM_ACTIVATE:
            if (LOWORD(msg.wp) == WA_INACTIVE && this->main_window)
            {
                ff::graphics::defer::full_screen(false);
            }
            break;

        case WM_SIZE:
            if (msg.wp != SIZE_MINIMIZED)
            {
                ff::graphics::defer::resize_target(this, this->window->size());
            }
            break;

        case WM_DESTROY:
            this->window_message_connection.disconnect();

            if (this->main_window)
            {
                ff::thread_dispatch::get_game()->send([this]()
                    {
                        this->was_full_screen_on_close = this->full_screen();
                        this->full_screen(false);
                    });
            }

            this->window = nullptr;
            break;

        case WM_SYSKEYDOWN:
            if (this->main_window && msg.wp == VK_RETURN) // ALT-ENTER to toggle full screen mode
            {
                ff::graphics::defer::full_screen(!this->full_screen());
                msg.result = 0;
                msg.handled = true;
            }
            else if (this->main_window && msg.wp == VK_BACK)
            {
#ifdef _DEBUG
                ff::graphics::defer::validate_device(true);
#endif
            }
            break;

        case WM_SYSCHAR:
            if (this->main_window && msg.wp == VK_RETURN)
            {
                // prevent a 'ding' sound when switching between modes
                msg.result = 0;
                msg.handled = true;
            }
            break;

#if !UWP_APP
        case WM_WINDOWPOSCHANGED:
            if (this->main_window)
            {
                const WINDOWPOS& wp = *reinterpret_cast<const WINDOWPOS*>(msg.lp);
                if ((wp.flags & SWP_FRAMECHANGED) != 0 && !::IsIconic(msg.hwnd))
                {
                    ff::graphics::defer::resize_target(this, this->window->size());
                }
            }
            break;
#endif
    }
}
