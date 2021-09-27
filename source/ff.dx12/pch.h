#pragma once

// C++
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <memory>
#include <mutex>

// Windows
#include <d3d12.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <dxgi1_6.h>
#include <wrl.h>

#if UWP_APP
#include <windows.ui.xaml.media.dxinterop.h>
#endif

// Vendor
#include <directxtex/DirectXTex/DirectXTex.h>
#include <directxtex/DirectXTex/d3dx12.h>

// FF
#include <ff.base.h>

// DirectX interface usage

using IDXGIAdapterX = typename IDXGIAdapter4;
using IDXGIDeviceX = typename IDXGIDevice4;
using IDXGIFactoryX = typename IDXGIFactory5; // 5 is the highest supported by the graphics debugger so far
using IDXGIOutputX = typename IDXGIOutput6;
using IDXGIResourceX = typename IDXGIResource1;
using IDXGISurfaceX = typename IDXGISurface2;
using IDXGISwapChainX = typename IDXGISwapChain4;

using ID3D12CommandAllocatorX = typename ID3D12CommandAllocator;
using ID3D12CommandListX = typename ID3D12CommandList;
using ID3D12CommandQueueX = typename ID3D12CommandQueue;
using ID3D12DescriptorHeapX = typename ID3D12DescriptorHeap;
using ID3D12DebugX = typename ID3D12Debug3;
using ID3D12DeviceX = typename ID3D12Device8;
using ID3D12FenceX = typename ID3D12Fence1;
using ID3D12GraphicsCommandListX = typename ID3D12GraphicsCommandList6;
using ID3D12HeapX = typename ID3D12Heap1;
using ID3D12PipelineStateX = typename ID3D12PipelineState;
using ID3D12ResourceX = typename ID3D12Resource2;
