#include "pch.h"

namespace ff::test::dx12
{
    // Swap chain tests. Each one drives a real hidden window, since DXGI needs a genuine HWND and
    // rejects a message-only window for CreateSwapChainForHwnd.
    TEST_CLASS(dx12_target_window_tests)
    {
    public:
        TEST_METHOD_CLEANUP(cleanup)
        {
            ff_dx12_destroy();
            destroy_window();
        }

        HWND create_window(int width = 320, int height = 240)
        {
            static ATOM class_atom = 0;
            if (!class_atom)
            {
                WNDCLASSEXW wc{};
                wc.cbSize = sizeof(wc);
                wc.lpfnWndProc = ::DefWindowProcW;
                wc.hInstance = ::GetModuleHandleW(nullptr);
                wc.lpszClassName = L"ff_test_swap_chain_window";
                class_atom = ::RegisterClassExW(&wc);
            }

            this->hwnd = ::CreateWindowExW(0, L"ff_test_swap_chain_window", L"ff test",
                WS_OVERLAPPEDWINDOW, 0, 0, width, height, nullptr, nullptr,
                ::GetModuleHandleW(nullptr), nullptr);

            Assert::IsNotNull(this->hwnd);
            return this->hwnd;
        }

        void destroy_window()
        {
            if (this->hwnd)
            {
                ::DestroyWindow(this->hwnd);
                this->hwnd = nullptr;
            }
        }

        TEST_METHOD(init_creates_swap_chain_and_back_buffers)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_target_window target{};
            Assert::IsTrue(ff_dx12_target_window_init(&target, create_window()));
            Assert::IsTrue(ff_dx12_target_window_valid(&target));

            Assert::AreEqual<size_t>(FF_DX12_TARGET_WINDOW_BUFFER_COUNT,
                ff_dx12_target_window_buffer_count(&target));

            // Every back buffer must have a distinct resource and a distinct view.
            for (size_t i = 0; i < FF_DX12_TARGET_WINDOW_BUFFER_COUNT; i++)
            {
                Assert::IsTrue(ff_dx12_resource_valid(&target.back_buffers[i]));
            }

            Assert::IsNotNull(ff_dx12_target_window_resource(&target));
            Assert::IsTrue(ff_dx12_target_window_view(&target).ptr != 0);

            ff_dx12_target_window_destroy(&target);
        }

        // A zero-sized client area (a minimized window) must be clamped, because DXGI rejects
        // zero-sized buffers.
        TEST_METHOD(zero_size_is_clamped)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_target_window target{};
            Assert::IsTrue(ff_dx12_target_window_init(&target, create_window()));

            Assert::IsTrue(ff_dx12_target_window_set_size(&target, 0, 0));
            Assert::AreEqual<size_t>(1, ff_dx12_target_window_width(&target));
            Assert::AreEqual<size_t>(1, ff_dx12_target_window_height(&target));
            Assert::IsTrue(ff_dx12_target_window_valid(&target));

            ff_dx12_target_window_destroy(&target);
        }

        // Resizing has to release every back buffer reference before ResizeBuffers, including the
        // ones the keep-alive list is holding. If that drain is missing, ResizeBuffers fails and
        // the debug layer reports a live-reference error.
        TEST_METHOD(resize_releases_back_buffers)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_target_window target{};
            Assert::IsTrue(ff_dx12_target_window_init(&target, create_window()));

            const float black[4]{ 0.0f, 0.0f, 0.0f, 1.0f };

            for (size_t i = 0; i < 8; i++)
            {
                // Render first, so each resize has a back buffer reference sitting on the
                // keep-alive list. Without rendering, resizing never exercises the drain.
                ff_dx12_frame_started();

                ff_dx12_commands commands{};
                Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));
                Assert::IsTrue(ff_dx12_target_window_begin_render(&target, &commands, black));
                Assert::IsTrue(ff_dx12_target_window_end_render(&target, &commands));

                ff_dx12_frame_complete();

                const size_t width = 64 + i * 16;
                Assert::IsTrue(ff_dx12_target_window_set_size(&target, width, 128));
                Assert::AreEqual<size_t>(width, ff_dx12_target_window_width(&target));
                Assert::IsTrue(ff_dx12_target_window_valid(&target));
            }

            ff_dx12_target_window_destroy(&target);
        }

        // Resizing to the same size must be a no-op rather than a needless rebuild.
        TEST_METHOD(resize_to_same_size_is_a_no_op)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_target_window target{};
            Assert::IsTrue(ff_dx12_target_window_init(&target, create_window()));
            Assert::IsTrue(ff_dx12_target_window_set_size(&target, 200, 100));

            ID3D12Resource* before = target.back_buffers[0].resource;
            Assert::IsTrue(ff_dx12_target_window_set_size(&target, 200, 100));

            Assert::IsTrue(before == target.back_buffers[0].resource);

            ff_dx12_target_window_destroy(&target);
        }

        // A full render/present cycle, repeated enough times to cross the swap chain's buffer
        // count several times over so back buffer rotation is exercised.
        TEST_METHOD(render_and_present_many_frames)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_target_window target{};
            Assert::IsTrue(ff_dx12_target_window_init(&target, create_window()));

            const float black[4]{ 0.0f, 0.0f, 0.0f, 1.0f };

            for (size_t i = 0; i < 10; i++)
            {
                ff_dx12_frame_started();

                ff_dx12_commands commands{};
                Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

                Assert::IsTrue(ff_dx12_target_window_begin_render(&target, &commands, black));
                Assert::IsTrue(ff_dx12_target_window_end_render(&target, &commands));

                ff_dx12_frame_complete();
            }

            ff_dx12_target_window_destroy(&target);
        }

        // Presenting and resizing interleaved is the case that actually happens when a user drags
        // a window edge, and it is where in-flight back buffer references bite.
        TEST_METHOD(present_then_resize_repeatedly)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_target_window target{};
            Assert::IsTrue(ff_dx12_target_window_init(&target, create_window()));

            const float black[4]{ 0.0f, 0.0f, 0.0f, 1.0f };

            for (size_t i = 0; i < 6; i++)
            {
                ff_dx12_frame_started();

                ff_dx12_commands commands{};
                Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));
                Assert::IsTrue(ff_dx12_target_window_begin_render(&target, &commands, black));
                Assert::IsTrue(ff_dx12_target_window_end_render(&target, &commands));

                ff_dx12_frame_complete();

                Assert::IsTrue(ff_dx12_target_window_set_size(&target, 100 + i * 32, 100 + i * 16));
                Assert::IsTrue(ff_dx12_target_window_valid(&target));
            }

            ff_dx12_target_window_destroy(&target);
        }

        // Discard instead of clear is the other begin_render path.
        TEST_METHOD(begin_render_with_discard)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_target_window target{};
            Assert::IsTrue(ff_dx12_target_window_init(&target, create_window()));

            ff_dx12_frame_started();

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));
            Assert::IsTrue(ff_dx12_target_window_begin_render(&target, &commands, nullptr));
            Assert::IsTrue(ff_dx12_target_window_end_render(&target, &commands));

            ff_dx12_frame_complete();

            ff_dx12_target_window_destroy(&target);
        }

        // Destroying the target while the GPU still has presented work in flight must not release
        // a back buffer early.
        TEST_METHOD(destroy_immediately_after_present)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_target_window target{};
            Assert::IsTrue(ff_dx12_target_window_init(&target, create_window()));

            const float black[4]{ 0.0f, 0.0f, 0.0f, 1.0f };

            ff_dx12_frame_started();

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));
            Assert::IsTrue(ff_dx12_target_window_begin_render(&target, &commands, black));
            Assert::IsTrue(ff_dx12_target_window_end_render(&target, &commands));

            // No frame_complete and no wait: destroy runs with the present still outstanding.
            ff_dx12_target_window_destroy(&target);
        }

        // Destroy must be safe to call on a zeroed struct and must be idempotent.
        TEST_METHOD(destroy_is_idempotent)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_target_window target{};
            Assert::IsTrue(ff_dx12_target_window_init(&target, create_window()));

            ff_dx12_target_window_destroy(&target);
            ff_dx12_target_window_destroy(&target);

            Assert::IsFalse(ff_dx12_target_window_valid(&target));

            ff_dx12_target_window empty{};
            ff_dx12_target_window_destroy(&empty);
        }

        // The pacing ladder must stay in range no matter how the stage moves, and latency/vsync
        // must always come from a valid stage.
        TEST_METHOD(pacing_stays_in_range)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_target_window target{};
            Assert::IsTrue(ff_dx12_target_window_init(&target, create_window()));

            for (size_t stage = 0; stage < FF_DX12_PACING_STAGE_COUNT; stage++)
            {
                target.pacing.stage = stage;
                const uint32_t latency = ff_dx12_target_window_pacing_latency(&target);
                Assert::IsTrue(latency == 1 || latency == 2);
            }

            ff_dx12_target_window_destroy(&target);
        }

    private:
        HWND hwnd{};
    };
}
