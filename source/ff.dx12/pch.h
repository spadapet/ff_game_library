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

// Vendor
#include <directxtex/DirectXTex/d3dx12.h>

// FF
#include <ff.base.h>
#include <ff.dxgi.h>

// DirectX interface usage

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

namespace ff::dx12
{
    using D3D_INPUT_ELEMENT_DESC = typename D3D12_INPUT_ELEMENT_DESC;
    static const D3D12_INPUT_CLASSIFICATION D3D_IPVA = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
}
