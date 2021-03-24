#include "pch.h"
#include "dx11_device_state.h"
#include "dx11_target_window.h"
#include "dxgi_util.h"
#include "graphics.h"
#include "texture_util.h"

ff::dx11_target_window::dx11_target_window()
    : dx11_target_window(ff::window::main())
{}

ff::dx11_target_window::dx11_target_window(ff::window* window)
    : window(window)
    , cached_size{}
    , main_window(ff::window::main() == window)
    , window_message_connection(window->message_sink().connect(std::bind(&dx11_target_window::handle_message, this, std::placeholders::_1)))
    , was_full_screen_on_close(false)
#if UWP_APP
    , use_xaml_composition(false)
    , cached_full_screen_uwp(false)
    , full_screen_uwp(false)
#endif
{
    this->size(this->window->size());

    ff::internal::graphics::add_child(this);

    if (this->main_window)
    {
        ff::graphics::defer::set_target(this);
    }
}

ff::dx11_target_window::~dx11_target_window()
{
    if (this->swap_chain)
    {
        this->swap_chain->SetFullscreenState(FALSE, nullptr);
    }

    if (this->main_window)
    {
        ff::graphics::defer::set_target(nullptr);
    }

    ff::internal::graphics::remove_child(this);
}

ff::dx11_target_window::operator bool() const
{
    return this->swap_chain && this->view_ && this->window;
}

DXGI_FORMAT ff::dx11_target_window::format() const
{
    return DXGI_FORMAT_B8G8R8A8_UNORM;
}

ff::window_size ff::dx11_target_window::size() const
{
    return this->cached_size;
}

ID3D11Texture2D* ff::dx11_target_window::texture()
{
    return this->texture_.Get();
}

ID3D11RenderTargetView* ff::dx11_target_window::view()
{
    return this->view_.Get();
}

bool ff::dx11_target_window::present(bool vsync)
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

bool ff::dx11_target_window::size(const ff::window_size& size)
{
    ff::window_size old_size = this->cached_size;
    ff::point_t<UINT> buffer_size = size.rotated_pixel_size().cast<UINT>();
    this->cached_size = size;
#if UWP_APP
    this->cached_full_screen_uwp = false;
#endif

    if (this->swap_chain && old_size != size) // on game thread
    {
        this->view_.Reset();
        this->texture_.Reset();
        ff::graphics::dx11_device_state().clear();

        DXGI_SWAP_CHAIN_DESC1 desc;
        this->swap_chain->GetDesc1(&desc);
        if (FAILED(this->swap_chain->ResizeBuffers(0, buffer_size.x, buffer_size.y, desc.Format, desc.Flags)))
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
        desc.BufferCount = 2;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.Scaling = DXGI_SCALING_STRETCH;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

        Microsoft::WRL::ComPtr<IDXGISwapChain1> new_swap_chain;
        Microsoft::WRL::ComPtr<IDXGIFactoryX> factory = ff::graphics::dxgi_factory_for_device();
        Microsoft::WRL::ComPtr<ID3D11DeviceX> device = ff::graphics::dx11_device();

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
        (!this->texture_ && FAILED(this->swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(this->texture_.GetAddressOf())))) ||
        (!this->view_ && (this->view_ = ff::internal::create_target_view(this->texture_.Get())) == nullptr) ||
#if UWP_APP
        (this->use_xaml_composition && FAILED(this->swap_chain->SetMatrixTransform(&inverse_scale))) ||
#endif
        FAILED(this->swap_chain->SetRotation(display_rotation)))
    {
        assert(false);
        return false;
    }

    this->size_changed_.notify(size);

    return true;
}

ff::signal_sink<ff::window_size>& ff::dx11_target_window::size_changed()
{
    return this->size_changed_;
}

bool ff::dx11_target_window::allow_full_screen() const
{
    return this->main_window;
}

bool ff::dx11_target_window::full_screen()
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

bool ff::dx11_target_window::full_screen(bool value)
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

bool ff::dx11_target_window::reset()
{
    BOOL full_screen = FALSE;
    if (this->swap_chain)
    {
        Microsoft::WRL::ComPtr<IDXGIOutput> output;
        this->swap_chain->GetFullscreenState(&full_screen, &output);
        this->swap_chain->SetFullscreenState(FALSE, nullptr);
        this->swap_chain.Reset();
    }

    this->view_.Reset();
    this->texture_.Reset();

    if (!this->window || !this->size(this->window->size()))
    {
        assert(false);
        return false;
    }

    if (this->swap_chain && full_screen)
    {
        this->swap_chain->SetFullscreenState(TRUE, nullptr);
    }

    return *this;
}

int ff::dx11_target_window::reset_priority() const
{
    return -100;
}

void ff::dx11_target_window::handle_message(ff::window_message& msg)
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
                ff::graphics::defer::resize_target(this->window->size());
            }
            break;

        case WM_DESTROY:
            ff::thread_dispatch::get_game()->send([this]()
                {
                    this->window_message_connection.disconnect();
                    this->was_full_screen_on_close = this->full_screen();
                    this->window = nullptr;
                });
            break;

        case WM_SYSKEYDOWN:
            if (msg.wp == VK_RETURN) // ALT-ENTER to toggle full screen mode
            {
                ff::graphics::defer::full_screen(!this->full_screen());
                msg.result = 0;
                msg.handled = true;
            }
            else if (msg.wp == VK_BACK)
            {
#ifdef _DEBUG
                ff::graphics::defer::validate_device(true);
#endif
            }
            break;

        case WM_SYSCHAR:
            if (msg.wp == VK_RETURN)
            {
                // prevent a 'ding' sound when switching between modes
                msg.result = 0;
                msg.handled = true;
            }
            break;

#if !UWP_APP
        case WM_WINDOWPOSCHANGED:
            {
                const WINDOWPOS& wp = *reinterpret_cast<const WINDOWPOS*>(msg.lp);
                if ((wp.flags & SWP_FRAMECHANGED) != 0)
                {
                    ff::graphics::defer::resize_target(this->window->size());
                }
            }
            break;

        case WM_DPICHANGED:
            {
                const RECT* rect = reinterpret_cast<const RECT*>(msg.lp);
                ::SetWindowPos(msg.hwnd, nullptr, rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top,
                    SWP_ASYNCWINDOWPOS | SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER);

                msg.result = 0;
                msg.handled = true;
            }
            break;
#endif
    }
}
