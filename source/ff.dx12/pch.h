#pragma once

// C++
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <memory>
#include <mutex>
#include <unordered_set>

// Windows
#include <d3d12.h>

#if UWP_APP
#include <windows.ui.xaml.media.dxinterop.h>
#endif

// Vendor
#include <directxtex/DirectXTex/d3dx12.h>

// FF
#include <ff.dxgi.h>
