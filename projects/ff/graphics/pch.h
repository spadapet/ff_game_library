#pragma once

// Windows
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <dwrite_3.h>
#include <dxgi1_6.h>

#if DXVER == 11
#include <d3d11_4.h>
#elif DXVER == 12
#include <d3d12.h>
#endif

#if UWP_APP
#include <windows.ui.xaml.media.dxinterop.h>
#endif

// Vendor
#include <directxtex/DirectXTex/DirectXTex.h>
#include <libpng/png.h>

// FF
#include <ff/base/base_api.h>
#include <ff/data/data_api.h>
#include <ff/resource/resource_api.h>

// DirectX interface usage

using IDXGIAdapterX = typename IDXGIAdapter4;
using IDXGIDeviceX = typename IDXGIDevice4;
using IDXGIFactoryX = typename IDXGIFactory5; // 5 is the highest supported by the graphics debugger so far
using IDXGIOutputX = typename IDXGIOutput6;
using IDXGIResourceX = typename IDXGIResource1;
using IDXGISurfaceX = typename IDXGISurface2;
using IDXGISwapChainX = typename IDXGISwapChain4;

using IDWriteFactoryX = typename IDWriteFactory7;
using IDWriteFontCollectionX = typename IDWriteFontCollection3;
using IDWriteFontFaceX = typename IDWriteFontFace5;
using IDWriteFontSetBuilderX = typename IDWriteFontSetBuilder2;
using IDWriteTextFormatX = typename IDWriteTextFormat3;
using IDWriteTextLayoutX = typename IDWriteTextLayout4;

#if DXVER == 11
using ID3D11DeviceX = typename ID3D11Device5;
using ID3D11DeviceContextX = typename ID3D11DeviceContext4;
#endif
