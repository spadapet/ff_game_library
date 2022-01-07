#pragma once

// Windows
#include <d3dcompiler.h>
#include <dwrite_3.h>

// Vendor
#include <directxtex/DirectXTex/DirectXTex.h>
#include <libpng/png.h>

// FF
#include <ff.dx12.h>
#include <ff.resource.h>

// DirectWrite interace usage

using IDWriteFactoryX = typename IDWriteFactory7;
using IDWriteFontCollectionX = typename IDWriteFontCollection3;
using IDWriteFontFaceX = typename IDWriteFontFace5;
using IDWriteFontSetBuilderX = typename IDWriteFontSetBuilder2;
using IDWriteTextFormatX = typename IDWriteTextFormat3;
using IDWriteTextLayoutX = typename IDWriteTextLayout4;
