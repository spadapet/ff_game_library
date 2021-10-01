#pragma once

// Windows
#include <d3dcompiler.h>
#include <dwrite_3.h>

#if UWP_APP
#include <windows.ui.xaml.media.dxinterop.h>
#endif

// Vendor
#include <directxtex/DirectXTex/DirectXTex.h>
#include <libpng/png.h>

// FF
#include <ff.base.h>
#include <ff.data.h>
#include <ff.resource.h>

#if DXVER == 11
#include <ff.dx11.h>
namespace ff_dx = ff::dx11;
namespace ff_internal_dx = ff::internal::dx11;
#elif DXVER == 12
#include <ff.dx12.h>
namespace ff_dx = ff::dx12;
namespace ff_internal_dx = ff::internal::dx12;
#endif

// DirectWrite interace usage

using IDWriteFactoryX = typename IDWriteFactory7;
using IDWriteFontCollectionX = typename IDWriteFontCollection3;
using IDWriteFontFaceX = typename IDWriteFontFace5;
using IDWriteFontSetBuilderX = typename IDWriteFontSetBuilder2;
using IDWriteTextFormatX = typename IDWriteTextFormat3;
using IDWriteTextLayoutX = typename IDWriteTextLayout4;
