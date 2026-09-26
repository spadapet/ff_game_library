#include "pch.h"

namespace ff::test::dx12
{
    // GPU events are debugger-only, so there is nothing observable to assert about the markers
    // themselves. What these tests protect is that the name table stays in sync with the enum and
    // that recording events does not break the command list around them. Note that D3D12 does not
    // validate marker blobs: a wrong blob size is silently accepted and only shows up as garbage
    // in a capture, so these tests cannot catch a malformed blob. They cover the call plumbing,
    // not the blob contents.
    TEST_CLASS(dx12_gpu_event_tests)
    {
    public:
        TEST_METHOD_CLEANUP(cleanup)
        {
            ff_dx12_destroy();
        }

        TEST_METHOD(every_event_has_a_name)
        {
            // The name table is indexed by enum value, so a new event added without a name would
            // otherwise read whatever follows the table.
            for (int i = ff_dx12_gpu_event_none + 1; i < ff_dx12_gpu_event_count; i++)
            {
                const ff_wstring_view name = ff_dx12_gpu_event_name((ff_dx12_gpu_event)i);

                Assert::IsNotNull(name.data);
                Assert::AreNotEqual<size_t>(0, name.count);
                Assert::AreEqual<size_t>(::wcslen(name.data), name.count);
            }
        }

        TEST_METHOD(event_names_are_distinct)
        {
            // Duplicate names make a PIX capture ambiguous, which defeats the point of the marker.
            for (int i = ff_dx12_gpu_event_none + 1; i < ff_dx12_gpu_event_count; i++)
            {
                for (int j = i + 1; j < ff_dx12_gpu_event_count; j++)
                {
                    const ff_wstring_view a = ff_dx12_gpu_event_name((ff_dx12_gpu_event)i);
                    const ff_wstring_view b = ff_dx12_gpu_event_name((ff_dx12_gpu_event)j);

                    Assert::IsFalse(ff_wstring_equal(a, b));
                }
            }
        }

        TEST_METHOD(nested_events_execute_cleanly)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            // Nesting is how the debugger builds its timeline tree. D3D12 accepts the markers
            // without validating them, so this confirms the calls are wired up and harmless
            // rather than that the capture looks right.
            ff_dx12_commands_begin_event(&commands, ff_dx12_gpu_event_render_frame);
            ff_dx12_commands_begin_event(&commands, ff_dx12_gpu_event_draw_2d);
            ff_dx12_commands_set_marker(&commands, ff_dx12_gpu_event_update_palette);
            ff_dx12_commands_begin_event(&commands, ff_dx12_gpu_event_draw_batch);
            ff_dx12_commands_end_event(&commands);
            ff_dx12_commands_end_event(&commands);
            ff_dx12_commands_end_event(&commands);

            ff_dx12_fence_value value = ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_fence_value_wait(value, nullptr);

            Assert::IsTrue(ff_dx12_device_valid());
        }

        TEST_METHOD(events_around_real_work_execute_cleanly)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc{};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            desc.Width = 64;
            desc.Height = 64;
            desc.DepthOrArraySize = 1;
            desc.MipLevels = 1;
            desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.SampleDesc.Count = 1;

            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("event texture"), &desc, nullptr));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            // Markers record into the same list as real commands. This is the case that would
            // catch a marker leaving the list unable to accept further commands.
            ff_dx12_commands_begin_event(&commands, ff_dx12_gpu_event_update_texture);
            ff_dx12_commands_resource_state(&commands, &resource, D3D12_RESOURCE_STATE_COPY_DEST, 0, 0, 0, 0);
            ff_dx12_commands_end_event(&commands);

            ff_dx12_fence_value value = ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_fence_value_wait(value, nullptr);

            Assert::IsTrue(ff_dx12_device_valid());

            ff_dx12_resource_destroy(&resource);
        }

        TEST_METHOD(events_on_invalid_commands_are_ignored)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            // Recording after the list was handed back must not dereference the dead cache.
            ff_dx12_commands commands{};

            ff_dx12_commands_begin_event(&commands, ff_dx12_gpu_event_render_frame);
            ff_dx12_commands_set_marker(&commands, ff_dx12_gpu_event_draw_2d);
            ff_dx12_commands_end_event(&commands);

            Assert::IsTrue(ff_dx12_device_valid());
        }
    };
}
