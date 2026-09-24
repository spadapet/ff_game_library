#include "pch.h"

namespace ff::test::dx12
{
    TEST_CLASS(dx12_frame_tests)
    {
    public:
        TEST_METHOD_CLEANUP(cleanup)
        {
            ff_dx12_destroy();
        }

        static D3D12_RESOURCE_DESC buffer_desc(UINT64 size = 1024)
        {
            D3D12_RESOURCE_DESC desc{};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            desc.Width = size;
            desc.Height = 1;
            desc.DepthOrArraySize = 1;
            desc.MipLevels = 1;
            desc.Format = DXGI_FORMAT_UNKNOWN;
            desc.SampleDesc.Count = 1;
            desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            return desc;
        }

        TEST_METHOD(frame_count_advances_on_complete)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            const uint64_t start = ff_dx12_frame_count();

            ff_dx12_frame_started();
            Assert::AreEqual(start, ff_dx12_frame_count());

            ff_dx12_frame_complete();
            Assert::AreEqual(start + 1, ff_dx12_frame_count());

            ff_dx12_frame_started();
            ff_dx12_frame_complete();
            Assert::AreEqual(start + 2, ff_dx12_frame_count());
        }

        TEST_METHOD(frame_count_resets_after_destroy)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));
            ff_dx12_frame_started();
            ff_dx12_frame_complete();
            Assert::IsTrue(ff_dx12_frame_count() > 0);

            ff_dx12_destroy();

            Assert::IsTrue(ff_dx12_init(nullptr));
            Assert::AreEqual((uint64_t)0, ff_dx12_frame_count());
        }

        // Destroying a resource that the GPU may still be reading must not block; the release is
        // deferred to the keep-alive list and drained later.
        TEST_METHOD(destroying_used_resource_defers_release)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = buffer_desc();
            ff_dx12_resource source{};
            ff_dx12_resource dest{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&source, FF_SVL("keep alive source"), &desc, nullptr));
            Assert::IsTrue(ff_dx12_resource_init_committed(&dest, FF_SVL("keep alive dest"), &desc, nullptr));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));
            ff_dx12_commands_copy_resource(&commands, &dest, &source);
            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);

            ff_dx12_resource_destroy(&source);
            ff_dx12_resource_destroy(&dest);

            Assert::IsFalse(ff_dx12_resource_valid(&source));
            Assert::IsFalse(ff_dx12_resource_valid(&dest));

            ff_dx12_wait_for_idle();
            ff_dx12_flush_keep_alive();
        }

        TEST_METHOD(frame_started_drains_keep_alive)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = buffer_desc();

            for (int i = 0; i < 8; i++)
            {
                ff_dx12_resource resource{};
                Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("frame resource"), &desc, nullptr));

                ff_dx12_commands commands{};
                Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));
                ff_dx12_commands_resource_state(&commands, &resource, D3D12_RESOURCE_STATE_COPY_SOURCE, 0, 0, 0, 0);
                ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);

                ff_dx12_resource_destroy(&resource);

                ff_dx12_frame_started();
                ff_dx12_frame_complete();
            }

            ff_dx12_wait_for_idle();
            ff_dx12_flush_keep_alive();
        }

        TEST_METHOD(flush_keep_alive_with_nothing_queued_is_harmless)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_flush_keep_alive();
            ff_dx12_flush_keep_alive();
            ff_dx12_wait_for_idle();
        }

        // Shutdown must release everything still queued, even without an explicit flush.
        TEST_METHOD(destroy_releases_pending_keep_alive)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = buffer_desc();
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("shutdown resource"), &desc, nullptr));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));
            ff_dx12_commands_resource_state(&commands, &resource, D3D12_RESOURCE_STATE_COPY_SOURCE, 0, 0, 0, 0);
            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);

            ff_dx12_resource_destroy(&resource);

            ff_dx12_destroy();
            Assert::IsTrue(ff_dx12_init(nullptr));
        }
    };
}
