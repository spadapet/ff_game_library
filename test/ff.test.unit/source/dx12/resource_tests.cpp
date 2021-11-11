#include "pch.h"
#include "test_base.h"

namespace ff::test::dx12
{
    TEST_CLASS(resource_tests), public ff::test::dx12::test_base
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

        TEST_METHOD(simple_buffer)
        {
            ff::dx12::resource r1(CD3DX12_RESOURCE_DESC::Buffer(128), D3D12_RESOURCE_STATE_COPY_DEST);
            Assert::IsTrue(r1);

            ff::dx12::resource r2(CD3DX12_RESOURCE_DESC::Buffer(64), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_CLEAR_VALUE{}, r1.mem_range());
            Assert::IsTrue(r2);
        }

        TEST_METHOD(simple_texture)
        {
            ff::dx12::resource r1(CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, 128, 128), D3D12_RESOURCE_STATE_COPY_DEST);
            Assert::IsTrue(r1);

            ff::dx12::resource r2(CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, 64, 64), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_CLEAR_VALUE{}, r1.mem_range());
            Assert::IsTrue(r2);
        }

        TEST_METHOD(update_buffer)
        {
            const std::array<int, 8> data{ 1, 2, 3, 4, 5, 6, 7, 8 };
            ff::dx12::resource r1(CD3DX12_RESOURCE_DESC::Buffer(ff::array_byte_size(data)), D3D12_RESOURCE_STATE_COPY_DEST);

            ff::dx12::fence_value fence_value = r1.update_buffer(nullptr, data.data(), 0, ff::array_byte_size(data));
            fence_value.wait(nullptr);

            std::vector<uint8_t> captured_data = r1.capture_buffer(nullptr, 0, ff::array_byte_size(data));
            Assert::AreEqual(ff::array_byte_size(data), ff::vector_byte_size(captured_data));
            Assert::AreEqual(0, std::memcmp(data.data(), captured_data.data(), ff::vector_byte_size(captured_data)));
        }

        TEST_METHOD(update_texture)
        {
            DirectX::ScratchImage scratch_source;
            DirectX::ScratchImage scratch_source_mips;
            DirectX::ScratchImage scratch_source_bc3;
            scratch_source.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 256, 256, 1, 1);
            {
                const DirectX::Image& source_image = *scratch_source.GetImages();
                uint32_t color = 0xFF000000;

                for (size_t y = 0; y < 256; y++)
                {
                    uint32_t* source_pixel = reinterpret_cast<uint32_t*>(source_image.pixels + y * source_image.rowPitch);
                    for (size_t x = 0; x < 256; x++)
                    {
                        *source_pixel++ = color++;
                    }
                }

                HRESULT hr = DirectX::GenerateMipMaps(scratch_source.GetImages(), 1, scratch_source.GetMetadata(), DirectX::TEX_FILTER_DEFAULT, 4, scratch_source_mips);
                Assert::IsTrue(SUCCEEDED(hr));

                hr = DirectX::Compress(scratch_source_mips.GetImages(), scratch_source_mips.GetImageCount(), scratch_source_mips.GetMetadata(), DXGI_FORMAT_BC3_UNORM, DirectX::TEX_COMPRESS_DEFAULT, 0, scratch_source_bc3);
                Assert::IsTrue(SUCCEEDED(hr));
            }

            ff::dx12::resource r1(CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, 256, 256, 1, 4), D3D12_RESOURCE_STATE_COPY_DEST);
            ff::dx12::fence_value fence_value = r1.update_texture(nullptr, scratch_source_mips.GetImages(), 0, 4, {});
            fence_value.wait(nullptr);

            DirectX::ScratchImage scratch_capture = r1.capture_texture(nullptr, 0, 4, nullptr);
            Assert::AreEqual(scratch_source_mips.GetImageCount(), scratch_capture.GetImageCount());
            Assert::AreEqual(
                ff::stable_hash_bytes(scratch_source_mips.GetPixels(), scratch_source_mips.GetPixelsSize()),
                ff::stable_hash_bytes(scratch_capture.GetPixels(), scratch_capture.GetPixelsSize()));

            ff::dx12::resource r2(r1, nullptr);
            DirectX::ScratchImage scratch_capture2 = r2.capture_texture(nullptr, 0, 4, nullptr);
            Assert::AreEqual(scratch_capture.GetImageCount(), scratch_capture2.GetImageCount());
            Assert::AreEqual(
                ff::stable_hash_bytes(scratch_capture.GetPixels(), scratch_capture.GetPixelsSize()),
                ff::stable_hash_bytes(scratch_capture2.GetPixels(), scratch_capture2.GetPixelsSize()));
        }
    };
}
