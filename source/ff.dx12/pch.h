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

// DirectX interface usage

using ID3D12CommandAllocatorX = typename ID3D12CommandAllocator;
using ID3D12CommandListX = typename ID3D12CommandList;
using ID3D12CommandQueueX = typename ID3D12CommandQueue;
using ID3D12DescriptorHeapX = typename ID3D12DescriptorHeap;
using ID3D12DebugX = typename ID3D12Debug3;
using ID3D12DeviceX = typename ID3D12Device8;
using ID3D12DeviceRemovedExtendedDataX = typename ID3D12DeviceRemovedExtendedData1;
using ID3D12FenceX = typename ID3D12Fence1;
using ID3D12GraphicsCommandListX = typename ID3D12GraphicsCommandList6;
using ID3D12HeapX = typename ID3D12Heap1;
using ID3D12PipelineStateX = typename ID3D12PipelineState;
using ID3D12ResourceX = typename ID3D12Resource2;
