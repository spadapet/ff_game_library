#pragma once

// Windows
#include <DirectXMath.h>
#include <dxgi1_6.h>
#include <wrl.h>

// Vendor
#include <directxtex/DirectXTex/DirectXTex.h>

// FF
#include <ff.base.h>

// DXGI interface usage

using IDXGIAdapterX = typename IDXGIAdapter4;
using IDXGIDeviceX = typename IDXGIDevice4;
using IDXGIFactoryX = typename IDXGIFactory5; // 5 is the highest supported by the graphics debugger so far
using IDXGIOutputX = typename IDXGIOutput6;
using IDXGIResourceX = typename IDXGIResource1;
using IDXGISurfaceX = typename IDXGISurface2;
using IDXGISwapChainX = typename IDXGISwapChain4;
