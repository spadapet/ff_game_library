#include "pch.h"
#include "test_base.h"

namespace
{
    class test_command_list : public ID3D12GraphicsCommandListX
    {
    public:
        std::vector<D3D12_RESOURCE_BARRIER> barriers;
        D3D12_COMMAND_LIST_TYPE type;

        test_command_list(D3D12_COMMAND_LIST_TYPE type)
            : type(type)
        {}

        virtual void STDMETHODCALLTYPE DispatchMesh(
            _In_  UINT ThreadGroupCountX,
            _In_  UINT ThreadGroupCountY,
            _In_  UINT ThreadGroupCountZ) override
        {}

        virtual void STDMETHODCALLTYPE RSSetShadingRate(
            _In_  D3D12_SHADING_RATE baseShadingRate,
            _In_reads_opt_(D3D12_RS_SET_SHADING_RATE_COMBINER_COUNT)  const D3D12_SHADING_RATE_COMBINER* combiners) override
        {}

        virtual void STDMETHODCALLTYPE RSSetShadingRateImage(
            _In_opt_  ID3D12Resource* shadingRateImage) override
        {}

        virtual void STDMETHODCALLTYPE BeginRenderPass(
            _In_  UINT NumRenderTargets,
            _In_reads_opt_(NumRenderTargets)  const D3D12_RENDER_PASS_RENDER_TARGET_DESC* pRenderTargets,
            _In_opt_  const D3D12_RENDER_PASS_DEPTH_STENCIL_DESC* pDepthStencil,
            D3D12_RENDER_PASS_FLAGS Flags) override
        {}

        virtual void STDMETHODCALLTYPE EndRenderPass() override {}

        virtual void STDMETHODCALLTYPE InitializeMetaCommand(
            _In_  ID3D12MetaCommand* pMetaCommand,
            _In_reads_bytes_opt_(InitializationParametersDataSizeInBytes)  const void* pInitializationParametersData,
            _In_  SIZE_T InitializationParametersDataSizeInBytes) override
        {}

        virtual void STDMETHODCALLTYPE ExecuteMetaCommand(
            _In_  ID3D12MetaCommand* pMetaCommand,
            _In_reads_bytes_opt_(ExecutionParametersDataSizeInBytes)  const void* pExecutionParametersData,
            _In_  SIZE_T ExecutionParametersDataSizeInBytes) override
        {}

        virtual void STDMETHODCALLTYPE BuildRaytracingAccelerationStructure(
            _In_  const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC* pDesc,
            _In_  UINT NumPostbuildInfoDescs,
            _In_reads_opt_(NumPostbuildInfoDescs)  const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC* pPostbuildInfoDescs) override
        {}

        virtual void STDMETHODCALLTYPE EmitRaytracingAccelerationStructurePostbuildInfo(
            _In_  const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC* pDesc,
            _In_  UINT NumSourceAccelerationStructures,
            _In_reads_(NumSourceAccelerationStructures)  const D3D12_GPU_VIRTUAL_ADDRESS* pSourceAccelerationStructureData) override
        {}

        virtual void STDMETHODCALLTYPE CopyRaytracingAccelerationStructure(
            _In_  D3D12_GPU_VIRTUAL_ADDRESS DestAccelerationStructureData,
            _In_  D3D12_GPU_VIRTUAL_ADDRESS SourceAccelerationStructureData,
            _In_  D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE Mode) override
        {}

        virtual void STDMETHODCALLTYPE SetPipelineState1(
            _In_  ID3D12StateObject* pStateObject) override
        {}

        virtual void STDMETHODCALLTYPE DispatchRays(
            _In_  const D3D12_DISPATCH_RAYS_DESC* pDesc) override
        {}

        virtual void STDMETHODCALLTYPE SetProtectedResourceSession(
            _In_opt_  ID3D12ProtectedResourceSession* pProtectedResourceSession) override
        {}

        virtual void STDMETHODCALLTYPE WriteBufferImmediate(
            UINT Count,
            _In_reads_(Count)  const D3D12_WRITEBUFFERIMMEDIATE_PARAMETER* pParams,
            _In_reads_opt_(Count)  const D3D12_WRITEBUFFERIMMEDIATE_MODE* pModes) override
        {}

        virtual void STDMETHODCALLTYPE AtomicCopyBufferUINT(
            _In_  ID3D12Resource* pDstBuffer,
            UINT64 DstOffset,
            _In_  ID3D12Resource* pSrcBuffer,
            UINT64 SrcOffset,
            UINT Dependencies,
            _In_reads_(Dependencies)  ID3D12Resource* const* ppDependentResources,
            _In_reads_(Dependencies)  const D3D12_SUBRESOURCE_RANGE_UINT64* pDependentSubresourceRanges) override
        {}

        virtual void STDMETHODCALLTYPE AtomicCopyBufferUINT64(
            _In_  ID3D12Resource* pDstBuffer,
            UINT64 DstOffset,
            _In_  ID3D12Resource* pSrcBuffer,
            UINT64 SrcOffset,
            UINT Dependencies,
            _In_reads_(Dependencies)  ID3D12Resource* const* ppDependentResources,
            _In_reads_(Dependencies)  const D3D12_SUBRESOURCE_RANGE_UINT64* pDependentSubresourceRanges) override
        {}

        virtual void STDMETHODCALLTYPE OMSetDepthBounds(
            _In_  FLOAT Min,
            _In_  FLOAT Max) override
        {}

        virtual void STDMETHODCALLTYPE SetSamplePositions(
            _In_  UINT NumSamplesPerPixel,
            _In_  UINT NumPixels,
            _In_reads_(NumSamplesPerPixel* NumPixels)  D3D12_SAMPLE_POSITION* pSamplePositions) override
        {}

        virtual void STDMETHODCALLTYPE ResolveSubresourceRegion(
            _In_  ID3D12Resource* pDstResource,
            _In_  UINT DstSubresource,
            _In_  UINT DstX,
            _In_  UINT DstY,
            _In_  ID3D12Resource* pSrcResource,
            _In_  UINT SrcSubresource,
            _In_opt_  D3D12_RECT* pSrcRect,
            _In_  DXGI_FORMAT Format,
            _In_  D3D12_RESOLVE_MODE ResolveMode) override
        {}

        virtual void STDMETHODCALLTYPE SetViewInstanceMask(
            _In_  UINT Mask) override
        {}

        virtual HRESULT STDMETHODCALLTYPE Close() override
        {
            return S_OK;
        }

        virtual HRESULT STDMETHODCALLTYPE Reset(
            _In_  ID3D12CommandAllocator* pAllocator,
            _In_opt_  ID3D12PipelineState* pInitialState) override
        {
            return S_OK;
        }

        virtual void STDMETHODCALLTYPE ClearState(
            _In_opt_  ID3D12PipelineState* pPipelineState) override
        {}

        virtual void STDMETHODCALLTYPE DrawInstanced(
            _In_  UINT VertexCountPerInstance,
            _In_  UINT InstanceCount,
            _In_  UINT StartVertexLocation,
            _In_  UINT StartInstanceLocation) override
        {}

        virtual void STDMETHODCALLTYPE DrawIndexedInstanced(
            _In_  UINT IndexCountPerInstance,
            _In_  UINT InstanceCount,
            _In_  UINT StartIndexLocation,
            _In_  INT BaseVertexLocation,
            _In_  UINT StartInstanceLocation) override
        {}

        virtual void STDMETHODCALLTYPE Dispatch(
            _In_  UINT ThreadGroupCountX,
            _In_  UINT ThreadGroupCountY,
            _In_  UINT ThreadGroupCountZ) override
        {}

        virtual void STDMETHODCALLTYPE CopyBufferRegion(
            _In_  ID3D12Resource* pDstBuffer,
            UINT64 DstOffset,
            _In_  ID3D12Resource* pSrcBuffer,
            UINT64 SrcOffset,
            UINT64 NumBytes) override
        {}

        virtual void STDMETHODCALLTYPE CopyTextureRegion(
            _In_  const D3D12_TEXTURE_COPY_LOCATION* pDst,
            UINT DstX,
            UINT DstY,
            UINT DstZ,
            _In_  const D3D12_TEXTURE_COPY_LOCATION* pSrc,
            _In_opt_  const D3D12_BOX* pSrcBox) override
        {}

        virtual void STDMETHODCALLTYPE CopyResource(
            _In_  ID3D12Resource* pDstResource,
            _In_  ID3D12Resource* pSrcResource) override
        {}

        virtual void STDMETHODCALLTYPE CopyTiles(
            _In_  ID3D12Resource* pTiledResource,
            _In_  const D3D12_TILED_RESOURCE_COORDINATE* pTileRegionStartCoordinate,
            _In_  const D3D12_TILE_REGION_SIZE* pTileRegionSize,
            _In_  ID3D12Resource* pBuffer,
            UINT64 BufferStartOffsetInBytes,
            D3D12_TILE_COPY_FLAGS Flags) override
        {}

        virtual void STDMETHODCALLTYPE ResolveSubresource(
            _In_  ID3D12Resource* pDstResource,
            _In_  UINT DstSubresource,
            _In_  ID3D12Resource* pSrcResource,
            _In_  UINT SrcSubresource,
            _In_  DXGI_FORMAT Format) override
        {}

        virtual void STDMETHODCALLTYPE IASetPrimitiveTopology(
            _In_  D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology) override
        {}

        virtual void STDMETHODCALLTYPE RSSetViewports(
            _In_range_(0, D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumViewports,
            _In_reads_(NumViewports)  const D3D12_VIEWPORT* pViewports) override
        {}

        virtual void STDMETHODCALLTYPE RSSetScissorRects(
            _In_range_(0, D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumRects,
            _In_reads_(NumRects)  const D3D12_RECT* pRects) override
        {}

        virtual void STDMETHODCALLTYPE OMSetBlendFactor(
            _In_reads_opt_(4)  const FLOAT BlendFactor[4]) override
        {}

        virtual void STDMETHODCALLTYPE OMSetStencilRef(
            _In_  UINT StencilRef) override
        {}

        virtual void STDMETHODCALLTYPE SetPipelineState(
            _In_  ID3D12PipelineState* pPipelineState) override
        {}

        virtual void STDMETHODCALLTYPE ResourceBarrier(
            _In_  UINT NumBarriers,
            _In_reads_(NumBarriers)  const D3D12_RESOURCE_BARRIER* pBarriers) override
        {
            for (UINT i = 0; i < NumBarriers; i++)
            {
                this->barriers.push_back(pBarriers[i]);
            }
        }

        virtual void STDMETHODCALLTYPE ExecuteBundle(
            _In_  ID3D12GraphicsCommandList* pCommandList) override
        {}

        virtual void STDMETHODCALLTYPE SetDescriptorHeaps(
            _In_  UINT NumDescriptorHeaps,
            _In_reads_(NumDescriptorHeaps)  ID3D12DescriptorHeap* const* ppDescriptorHeaps) override
        {}

        virtual void STDMETHODCALLTYPE SetComputeRootSignature(
            _In_opt_  ID3D12RootSignature* pRootSignature) override
        {}

        virtual void STDMETHODCALLTYPE SetGraphicsRootSignature(
            _In_opt_  ID3D12RootSignature* pRootSignature) override
        {}

        virtual void STDMETHODCALLTYPE SetComputeRootDescriptorTable(
            _In_  UINT RootParameterIndex,
            _In_  D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor) override
        {}

        virtual void STDMETHODCALLTYPE SetGraphicsRootDescriptorTable(
            _In_  UINT RootParameterIndex,
            _In_  D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor) override
        {}

        virtual void STDMETHODCALLTYPE SetComputeRoot32BitConstant(
            _In_  UINT RootParameterIndex,
            _In_  UINT SrcData,
            _In_  UINT DestOffsetIn32BitValues) override
        {}

        virtual void STDMETHODCALLTYPE SetGraphicsRoot32BitConstant(
            _In_  UINT RootParameterIndex,
            _In_  UINT SrcData,
            _In_  UINT DestOffsetIn32BitValues) override
        {}

        virtual void STDMETHODCALLTYPE SetComputeRoot32BitConstants(
            _In_  UINT RootParameterIndex,
            _In_  UINT Num32BitValuesToSet,
            _In_reads_(Num32BitValuesToSet * sizeof(UINT))  const void* pSrcData,
            _In_  UINT DestOffsetIn32BitValues) override
        {}

        virtual void STDMETHODCALLTYPE SetGraphicsRoot32BitConstants(
            _In_  UINT RootParameterIndex,
            _In_  UINT Num32BitValuesToSet,
            _In_reads_(Num32BitValuesToSet * sizeof(UINT))  const void* pSrcData,
            _In_  UINT DestOffsetIn32BitValues) override
        {}

        virtual void STDMETHODCALLTYPE SetComputeRootConstantBufferView(
            _In_  UINT RootParameterIndex,
            _In_  D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) override
        {}

        virtual void STDMETHODCALLTYPE SetGraphicsRootConstantBufferView(
            _In_  UINT RootParameterIndex,
            _In_  D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) override
        {}

        virtual void STDMETHODCALLTYPE SetComputeRootShaderResourceView(
            _In_  UINT RootParameterIndex,
            _In_  D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) override
        {}

        virtual void STDMETHODCALLTYPE SetGraphicsRootShaderResourceView(
            _In_  UINT RootParameterIndex,
            _In_  D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) override
        {}

        virtual void STDMETHODCALLTYPE SetComputeRootUnorderedAccessView(
            _In_  UINT RootParameterIndex,
            _In_  D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) override
        {}

        virtual void STDMETHODCALLTYPE SetGraphicsRootUnorderedAccessView(
            _In_  UINT RootParameterIndex,
            _In_  D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) override
        {}

        virtual void STDMETHODCALLTYPE IASetIndexBuffer(
            _In_opt_  const D3D12_INDEX_BUFFER_VIEW* pView) override
        {}

        virtual void STDMETHODCALLTYPE IASetVertexBuffers(
            _In_  UINT StartSlot,
            _In_  UINT NumViews,
            _In_reads_opt_(NumViews)  const D3D12_VERTEX_BUFFER_VIEW* pViews) override
        {}

        virtual void STDMETHODCALLTYPE SOSetTargets(
            _In_  UINT StartSlot,
            _In_  UINT NumViews,
            _In_reads_opt_(NumViews)  const D3D12_STREAM_OUTPUT_BUFFER_VIEW* pViews) override
        {}

        virtual void STDMETHODCALLTYPE OMSetRenderTargets(
            _In_  UINT NumRenderTargetDescriptors,
            _In_opt_  const D3D12_CPU_DESCRIPTOR_HANDLE* pRenderTargetDescriptors,
            _In_  BOOL RTsSingleHandleToDescriptorRange,
            _In_opt_  const D3D12_CPU_DESCRIPTOR_HANDLE* pDepthStencilDescriptor) override
        {}

        virtual void STDMETHODCALLTYPE ClearDepthStencilView(
            _In_  D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView,
            _In_  D3D12_CLEAR_FLAGS ClearFlags,
            _In_  FLOAT Depth,
            _In_  UINT8 Stencil,
            _In_  UINT NumRects,
            _In_reads_(NumRects)  const D3D12_RECT* pRects) override
        {}

        virtual void STDMETHODCALLTYPE ClearRenderTargetView(
            _In_  D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView,
            _In_  const FLOAT ColorRGBA[4],
            _In_  UINT NumRects,
            _In_reads_(NumRects)  const D3D12_RECT* pRects) override
        {}

        virtual void STDMETHODCALLTYPE ClearUnorderedAccessViewUint(
            _In_  D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap,
            _In_  D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle,
            _In_  ID3D12Resource* pResource,
            _In_  const UINT Values[4],
            _In_  UINT NumRects,
            _In_reads_(NumRects)  const D3D12_RECT* pRects) override
        {}

        virtual void STDMETHODCALLTYPE ClearUnorderedAccessViewFloat(
            _In_  D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap,
            _In_  D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle,
            _In_  ID3D12Resource* pResource,
            _In_  const FLOAT Values[4],
            _In_  UINT NumRects,
            _In_reads_(NumRects)  const D3D12_RECT* pRects) override
        {}

        virtual void STDMETHODCALLTYPE DiscardResource(
            _In_  ID3D12Resource* pResource,
            _In_opt_  const D3D12_DISCARD_REGION* pRegion) override
        {}

        virtual void STDMETHODCALLTYPE BeginQuery(
            _In_  ID3D12QueryHeap* pQueryHeap,
            _In_  D3D12_QUERY_TYPE Type,
            _In_  UINT Index) override
        {}

        virtual void STDMETHODCALLTYPE EndQuery(
            _In_  ID3D12QueryHeap* pQueryHeap,
            _In_  D3D12_QUERY_TYPE Type,
            _In_  UINT Index) override
        {}

        virtual void STDMETHODCALLTYPE ResolveQueryData(
            _In_  ID3D12QueryHeap* pQueryHeap,
            _In_  D3D12_QUERY_TYPE Type,
            _In_  UINT StartIndex,
            _In_  UINT NumQueries,
            _In_  ID3D12Resource* pDestinationBuffer,
            _In_  UINT64 AlignedDestinationBufferOffset) override
        {}

        virtual void STDMETHODCALLTYPE SetPredication(
            _In_opt_  ID3D12Resource* pBuffer,
            _In_  UINT64 AlignedBufferOffset,
            _In_  D3D12_PREDICATION_OP Operation) override
        {}

        virtual void STDMETHODCALLTYPE SetMarker(
            UINT Metadata,
            _In_reads_bytes_opt_(Size)  const void* pData,
            UINT Size) override
        {}

        virtual void STDMETHODCALLTYPE BeginEvent(
            UINT Metadata,
            _In_reads_bytes_opt_(Size)  const void* pData,
            UINT Size) override
        {}

        virtual void STDMETHODCALLTYPE EndEvent() override {}

        virtual void STDMETHODCALLTYPE ExecuteIndirect(
            _In_  ID3D12CommandSignature* pCommandSignature,
            _In_  UINT MaxCommandCount,
            _In_  ID3D12Resource* pArgumentBuffer,
            _In_  UINT64 ArgumentBufferOffset,
            _In_opt_  ID3D12Resource* pCountBuffer,
            _In_  UINT64 CountBufferOffset) override
        {}

        virtual D3D12_COMMAND_LIST_TYPE STDMETHODCALLTYPE GetType() override
        {
            return this->type;
        }

        virtual HRESULT STDMETHODCALLTYPE GetDevice(
            REFIID riid,
            _COM_Outptr_opt_  void** ppvDevice) override
        {
            return E_FAIL;
        }

        virtual HRESULT STDMETHODCALLTYPE GetPrivateData(
            _In_  REFGUID guid,
            _Inout_  UINT* pDataSize,
            _Out_writes_bytes_opt_(*pDataSize)  void* pData) override
        {
            return E_FAIL;
        }

        virtual HRESULT STDMETHODCALLTYPE SetPrivateData(
            _In_  REFGUID guid,
            _In_  UINT DataSize,
            _In_reads_bytes_opt_(DataSize)  const void* pData) override
        {
            return E_FAIL;
        }

        virtual HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
            _In_  REFGUID guid,
            _In_opt_  const IUnknown* pData) override
        {
            return E_FAIL;
        }

        virtual HRESULT STDMETHODCALLTYPE SetName(
            _In_z_  LPCWSTR Name) override
        {
            return E_FAIL;
        }

        virtual HRESULT STDMETHODCALLTYPE QueryInterface(
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR* __RPC_FAR* ppvObject) override
        {
            return E_FAIL;
        }

        virtual ULONG STDMETHODCALLTYPE AddRef(void) override
        {
            return 1;
        }

        virtual ULONG STDMETHODCALLTYPE Release(void) override
        {
            return 1;
        }
    };
}

namespace ff::test::dx12
{
    TEST_CLASS(resource_tracker_tests), public ff::test::dx12::test_base
    {
    public:
        TEST_METHOD_INITIALIZE(initialize)
        {
            this->test_initialize();
        }

        TEST_METHOD_CLEANUP(cleanup)
        {
            this->test_cleanup();
        }

        TEST_METHOD(resource_tracker_simple_copy)
        {
            this->resource_tracker_simple(D3D12_COMMAND_LIST_TYPE_COPY);
        }

        TEST_METHOD(resource_tracker_simple_direct)
        {
            this->resource_tracker_simple(D3D12_COMMAND_LIST_TYPE_DIRECT);
        }

        TEST_METHOD(resource_tracker_no_promote)
        {
            const ff::point_size size(32, 32);
            ff::dx12::depth depth1(size);

            ::test_command_list command_list(D3D12_COMMAND_LIST_TYPE_DIRECT);
            ::test_command_list command_list_before(D3D12_COMMAND_LIST_TYPE_DIRECT);

            ff::dx12::resource_tracker tracker;
            tracker.state(*depth1.resource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, 0, 1, 0, 1);
            tracker.state(*depth1.resource(), D3D12_RESOURCE_STATE_DEPTH_READ, 0, 1, 0, 1);

            tracker.flush(&command_list);
            tracker.close(&command_list_before, nullptr, nullptr);

            Assert::AreEqual<UINT>(D3D12_RESOURCE_STATE_DEPTH_READ, depth1.resource()->global_state().get(0).first);

            Assert::AreEqual<size_t>(1, command_list.barriers.size());

            Assert::AreEqual<UINT>(D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, command_list.barriers[0].Type);
            Assert::AreEqual<UINT>(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, command_list.barriers[0].Transition.Subresource);
            Assert::AreEqual<UINT>(D3D12_RESOURCE_STATE_DEPTH_WRITE, command_list.barriers[0].Transition.StateBefore);
            Assert::AreEqual<UINT>(D3D12_RESOURCE_STATE_DEPTH_READ, command_list.barriers[0].Transition.StateAfter);

            Assert::AreEqual<size_t>(1, command_list_before.barriers.size());

            Assert::AreEqual<UINT>(D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, command_list_before.barriers[0].Type);
            Assert::AreEqual<UINT>(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, command_list_before.barriers[0].Transition.Subresource);
            Assert::AreEqual<UINT>(D3D12_RESOURCE_STATE_COMMON, command_list_before.barriers[0].Transition.StateBefore);
            Assert::AreEqual<UINT>(D3D12_RESOURCE_STATE_DEPTH_WRITE, command_list_before.barriers[0].Transition.StateAfter);
        }

        TEST_METHOD(resource_tracker_multi_list_promote)
        {
            ff::dx12::resource res[3]
            {
                { "", CD3DX12_RESOURCE_DESC::Buffer(32) },
                { "", CD3DX12_RESOURCE_DESC::Buffer(32) },
                { "", CD3DX12_RESOURCE_DESC::Buffer(32) },
            };

            ::test_command_list lists[3]{ D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_TYPE_DIRECT };
            ::test_command_list before_lists[3]{ D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_TYPE_DIRECT };

            ff::dx12::resource_tracker trackers[3];
            trackers[0].state(res[0], D3D12_RESOURCE_STATE_COPY_SOURCE);
            trackers[1].state(res[1], D3D12_RESOURCE_STATE_COPY_DEST);
            trackers[2].state(res[1], D3D12_RESOURCE_STATE_COPY_DEST);
            trackers[2].state(res[2], D3D12_RESOURCE_STATE_INDEX_BUFFER);

            for (size_t i = 0; i < 3; i++)
            {
                trackers[i].flush(&lists[i]);
            }

            for (size_t i = 0; i < 3; i++)
            {
                trackers[i].close(&before_lists[i], i ? &trackers[i - 1] : nullptr, (i + 1 < 3) ? &trackers[i + 1] : nullptr);
            }

            Assert::AreEqual<UINT>(D3D12_RESOURCE_STATE_COMMON, res[0].global_state().get(0).first);
            Assert::AreEqual<UINT>(D3D12_RESOURCE_STATE_COMMON, res[1].global_state().get(0).first);
            Assert::AreEqual<UINT>(D3D12_RESOURCE_STATE_COMMON, res[2].global_state().get(0).first);
        }

        TEST_METHOD(resource_tracker_multi_list_barrier)
        {
            ff::dx12::resource res[3]
            {
                { "", CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, 32, 32, 1, 1) },
                { "", CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, 32, 32, 1, 1) },
                { "", CD3DX12_RESOURCE_DESC::Buffer(32) },
            };

            ::test_command_list lists[3]{ D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_TYPE_DIRECT };
            ::test_command_list before_lists[3]{ D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_TYPE_DIRECT };

            ff::dx12::resource_tracker trackers[3];
            trackers[0].state(res[0], D3D12_RESOURCE_STATE_COPY_SOURCE);
            trackers[2].state(res[0], D3D12_RESOURCE_STATE_COPY_DEST);
            trackers[2].state(res[0], D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
            trackers[1].state(res[1], D3D12_RESOURCE_STATE_COPY_DEST);
            trackers[2].state(res[1], D3D12_RESOURCE_STATE_INDEX_BUFFER);
            trackers[1].state(res[2], D3D12_RESOURCE_STATE_COPY_DEST);
            trackers[2].state(res[2], D3D12_RESOURCE_STATE_INDEX_BUFFER);

            for (size_t i = 0; i < 3; i++)
            {
                trackers[i].flush(&lists[i]);
            }

            for (size_t i = 0; i < 3; i++)
            {
                trackers[i].close(&before_lists[i], i ? &trackers[i - 1] : nullptr, (i + 1 < 3) ? &trackers[i + 1] : nullptr);
            }

            Assert::AreEqual<UINT>(D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, res[0].global_state().get(0).first);
            Assert::AreEqual<UINT>(D3D12_RESOURCE_STATE_INDEX_BUFFER, res[1].global_state().get(0).first);
            Assert::AreEqual<UINT>(D3D12_RESOURCE_STATE_COMMON, res[2].global_state().get(0).first);

            Assert::AreEqual<size_t>(0, lists[0].barriers.size());
            Assert::AreEqual<size_t>(0, lists[1].barriers.size());
            Assert::AreEqual<size_t>(1, lists[2].barriers.size());

            Assert::AreEqual<size_t>(0, before_lists[0].barriers.size());
            Assert::AreEqual<size_t>(0, before_lists[1].barriers.size());
            Assert::AreEqual<size_t>(3, before_lists[2].barriers.size());

            Assert::AreEqual<UINT>(D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, lists[2].barriers[0].Type);
            Assert::AreEqual<UINT>(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, lists[2].barriers[0].Transition.Subresource);
            Assert::AreEqual<UINT>(D3D12_RESOURCE_STATE_COPY_DEST, lists[2].barriers[0].Transition.StateBefore);
            Assert::AreEqual<UINT>(D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, lists[2].barriers[0].Transition.StateAfter);

            Assert::AreEqual<UINT>(D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, before_lists[2].barriers[0].Type);
            Assert::AreEqual<UINT>(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, before_lists[2].barriers[0].Transition.Subresource);
            Assert::AreEqual<UINT>(D3D12_RESOURCE_STATE_COPY_SOURCE, before_lists[2].barriers[0].Transition.StateBefore);
            Assert::AreEqual<UINT>(D3D12_RESOURCE_STATE_COPY_DEST, before_lists[2].barriers[0].Transition.StateAfter);

            Assert::AreEqual<UINT>(D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, before_lists[2].barriers[1].Type);
            Assert::AreEqual<UINT>(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, before_lists[2].barriers[1].Transition.Subresource);
            Assert::AreEqual<UINT>(D3D12_RESOURCE_STATE_COPY_DEST, before_lists[2].barriers[1].Transition.StateBefore);
            Assert::AreEqual<UINT>(D3D12_RESOURCE_STATE_INDEX_BUFFER, before_lists[2].barriers[1].Transition.StateAfter);

            Assert::AreEqual<UINT>(D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, before_lists[2].barriers[2].Type);
            Assert::AreEqual<UINT>(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, before_lists[2].barriers[2].Transition.Subresource);
            Assert::AreEqual<UINT>(D3D12_RESOURCE_STATE_COPY_DEST, before_lists[2].barriers[2].Transition.StateBefore);
            Assert::AreEqual<UINT>(D3D12_RESOURCE_STATE_INDEX_BUFFER, before_lists[2].barriers[2].Transition.StateAfter);
        }

    private:
        void resource_tracker_simple(D3D12_COMMAND_LIST_TYPE type)
        {
            const ff::point_size size(32, 32);
            ff::dx12::texture tex1(size, DXGI_FORMAT_R8G8B8A8_UNORM);
            ff::dx12::texture tex2(size, DXGI_FORMAT_R8G8B8A8_UNORM, 2, 2);

            ff::dx12::resource_tracker tracker;
            tracker.state(*tex1.dx12_resource(), D3D12_RESOURCE_STATE_COPY_DEST, 0, 1, 0, 1);
            tracker.state(*tex2.dx12_resource(), D3D12_RESOURCE_STATE_COPY_SOURCE, 1, 1, 0, 2);
            tracker.state(*tex1.dx12_resource(), D3D12_RESOURCE_STATE_COPY_SOURCE, 0, 1, 0, 1);
            tracker.state(*tex2.dx12_resource(), D3D12_RESOURCE_STATE_COPY_DEST, 0, 2, 0, 1);

            ::test_command_list command_list(type);
            ::test_command_list command_list_before(type);
            tracker.flush(&command_list);
            tracker.close(&command_list_before, nullptr, nullptr);

            Assert::AreEqual(tex1.dx12_resource()->sub_resource_size(), tex1.dx12_resource()->global_state().sub_resource_size());
            Assert::AreEqual(tex2.dx12_resource()->sub_resource_size(), tex2.dx12_resource()->global_state().sub_resource_size());

            bool decayed = (type == D3D12_COMMAND_LIST_TYPE_COPY);
            Assert::AreEqual<UINT>(decayed ? D3D12_RESOURCE_STATE_COMMON : D3D12_RESOURCE_STATE_COPY_SOURCE, tex1.dx12_resource()->global_state().get(0).first);
            Assert::AreEqual<UINT>(decayed ? D3D12_RESOURCE_STATE_COMMON : D3D12_RESOURCE_STATE_COPY_DEST, tex2.dx12_resource()->global_state().get(0).first);
            Assert::AreEqual<UINT>(decayed ? D3D12_RESOURCE_STATE_COMMON : D3D12_RESOURCE_STATE_COPY_DEST, tex2.dx12_resource()->global_state().get(2).first);
            Assert::AreEqual<UINT>(D3D12_RESOURCE_STATE_COMMON, tex2.dx12_resource()->global_state().get(1).first);
            Assert::AreEqual<UINT>(D3D12_RESOURCE_STATE_COMMON, tex2.dx12_resource()->global_state().get(3).first);

            Assert::AreEqual<size_t>(2, command_list.barriers.size());

            Assert::AreEqual<UINT>(D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, command_list.barriers[0].Type);
            Assert::AreEqual<UINT>(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, command_list.barriers[0].Transition.Subresource);
            Assert::AreEqual<UINT>(D3D12_RESOURCE_STATE_COPY_DEST, command_list.barriers[0].Transition.StateBefore);
            Assert::AreEqual<UINT>(D3D12_RESOURCE_STATE_COPY_SOURCE, command_list.barriers[0].Transition.StateAfter);

            Assert::AreEqual<UINT>(D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, command_list.barriers[1].Type);
            Assert::AreEqual<UINT>(2, command_list.barriers[1].Transition.Subresource);
            Assert::AreEqual<UINT>(D3D12_RESOURCE_STATE_COPY_SOURCE, command_list.barriers[1].Transition.StateBefore);
            Assert::AreEqual<UINT>(D3D12_RESOURCE_STATE_COPY_DEST, command_list.barriers[1].Transition.StateAfter);
        }
    };
}
