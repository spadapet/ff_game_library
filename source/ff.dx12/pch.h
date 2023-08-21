#pragma once
#pragma push_macro("assert")

// C++
#include <algorithm>
#include <unordered_set>

// Windows
#include <d3d12.h>

#if UWP_APP
#include <windows.ui.xaml.media.dxinterop.h>
#endif

// Vendor
#include <directxtex/DirectXTex/d3dx12.h>

// FF
#pragma pop_macro("assert")
#include <ff.dxgi.h>
