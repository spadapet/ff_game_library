#pragma once
#pragma push_macro("assert")

// C++
#include <algorithm>
#include <fstream>

// Windows
#include <DirectXMath.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dwrite_3.h>
#include <dxgi1_6.h>
#include <mfapi.h>
#include <Mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <windowsx.h>
#include <xaudio2.h>
#include <Xinput.h>

// Vendor
#include <directxtex/Common/d3dx12.h>
#include <directxtex/DirectXTex/DirectXTex.h>
#include <libpng/png.h>

#ifdef BUILDING_FF_APPLICATION
#include <pix3.h>
#endif

#if USE_IMGUI
#include <imgui/imgui.h>
#endif

// FF
#pragma pop_macro("assert")
#include <ff.base.h>
