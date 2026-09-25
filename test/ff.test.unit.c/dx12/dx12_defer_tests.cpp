#include "pch.h"

#include <thread>
#include <vector>

namespace ff::test::dx12
{
    // The deferred work queue is what lets the window thread ask for a resize or a device reset
    // without touching DX12 objects the render thread owns. These tests drive it with a real
    // swap chain, since a resize that never reaches the swap chain proves nothing.
    TEST_CLASS(dx12_defer_tests)
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
                wc.lpszClassName = L"ff_test_defer_window";
                class_atom = ::RegisterClassExW(&wc);
            }

            this->hwnd = ::CreateWindowExW(0, L"ff_test_defer_window", L"ff test",
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

        TEST_METHOD(nothing_deferred_by_default)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));
            Assert::IsFalse(ff_dx12_has_deferred());

            // Flushing an empty queue is a successful no-op, so a caller can flush unconditionally.
            Assert::IsTrue(ff_dx12_flush_deferred());
        }

        TEST_METHOD(deferred_resize_is_not_applied_until_flush)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_target_window target{};
            Assert::IsTrue(ff_dx12_target_window_init(&target, create_window()));

            const size_t original_width = ff_dx12_target_window_width(&target);

            ff_dx12_defer_resize_target(&target, 640, 480);

            // The whole point: queuing must not touch the swap chain, because the owning thread
            // could be mid-frame.
            Assert::AreEqual(original_width, ff_dx12_target_window_width(&target));
            Assert::IsTrue(ff_dx12_has_deferred());

            Assert::IsTrue(ff_dx12_flush_deferred());

            Assert::AreEqual<size_t>(640, ff_dx12_target_window_width(&target));
            Assert::AreEqual<size_t>(480, ff_dx12_target_window_height(&target));
            Assert::IsFalse(ff_dx12_has_deferred());
            Assert::IsTrue(ff_dx12_target_window_valid(&target));

            ff_dx12_target_window_destroy(&target);
        }

        TEST_METHOD(repeated_resizes_coalesce_to_the_last_one)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_target_window target{};
            Assert::IsTrue(ff_dx12_target_window_init(&target, create_window()));

            // Dragging a window edge produces a flood of these. Only the final size should reach
            // the swap chain, since each real resize waits for idle.
            for (size_t i = 1; i <= 20; i++)
            {
                ff_dx12_defer_resize_target(&target, 300 + i, 200 + i);
            }

            Assert::IsTrue(ff_dx12_flush_deferred());

            Assert::AreEqual<size_t>(320, ff_dx12_target_window_width(&target));
            Assert::AreEqual<size_t>(220, ff_dx12_target_window_height(&target));

            ff_dx12_target_window_destroy(&target);
        }

        TEST_METHOD(destroying_a_target_cancels_its_deferred_resize)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_target_window target{};
            Assert::IsTrue(ff_dx12_target_window_init(&target, create_window()));

            ff_dx12_defer_resize_target(&target, 640, 480);
            Assert::IsTrue(ff_dx12_has_deferred());

            // Without the cancel inside destroy, the queue would still hold this pointer and the
            // flush below would resize freed memory.
            ff_dx12_target_window_destroy(&target);
            Assert::IsFalse(ff_dx12_has_deferred());

            Assert::IsTrue(ff_dx12_flush_deferred());
        }

        TEST_METHOD(cancel_only_removes_the_named_target)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_target_window target{};
            Assert::IsTrue(ff_dx12_target_window_init(&target, create_window()));

            // A second, unrelated pointer stands in for another window. It is never flushed,
            // so it only has to be distinct.
            ff_dx12_target_window other{};

            ff_dx12_defer_resize_target(&other, 100, 100);
            ff_dx12_defer_resize_target(&target, 640, 480);

            ff_dx12_cancel_deferred_target(&other);
            Assert::IsTrue(ff_dx12_has_deferred());

            Assert::IsTrue(ff_dx12_flush_deferred());
            Assert::AreEqual<size_t>(640, ff_dx12_target_window_width(&target));

            ff_dx12_target_window_destroy(&target);
        }

        TEST_METHOD(deferred_reset_rebuilds_the_swap_chain)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_target_window target{};
            Assert::IsTrue(ff_dx12_target_window_init(&target, create_window()));

            const uint64_t reset_count_before = ff_dx12_device_reset_count();

            ff_dx12_defer_reset_device(true);
            Assert::IsTrue(ff_dx12_has_deferred());

            // Queuing must not reset anything on its own.
            Assert::AreEqual(reset_count_before, ff_dx12_device_reset_count());

            Assert::IsTrue(ff_dx12_flush_deferred());

            Assert::AreEqual(reset_count_before + 1, ff_dx12_device_reset_count());
            Assert::IsTrue(ff_dx12_target_window_valid(&target));
            Assert::IsFalse(ff_dx12_has_deferred());

            ff_dx12_target_window_destroy(&target);
        }

        TEST_METHOD(reset_and_resize_in_one_flush_leaves_the_new_size)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_target_window target{};
            Assert::IsTrue(ff_dx12_target_window_init(&target, create_window()));

            // A reset rebuilds every swap chain at its current size, so it has to be applied
            // before the resize or the new size would be thrown away.
            ff_dx12_defer_resize_target(&target, 640, 480);
            ff_dx12_defer_reset_device(true);

            Assert::IsTrue(ff_dx12_flush_deferred());

            Assert::AreEqual<size_t>(640, ff_dx12_target_window_width(&target));
            Assert::AreEqual<size_t>(480, ff_dx12_target_window_height(&target));
            Assert::IsTrue(ff_dx12_target_window_valid(&target));

            ff_dx12_target_window_destroy(&target);
        }

        TEST_METHOD(resize_can_be_queued_from_another_thread)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_target_window target{};
            Assert::IsTrue(ff_dx12_target_window_init(&target, create_window()));

            // This is the case the queue exists for: the window thread queues while the owning
            // thread is elsewhere. Only the queue is touched off-thread; the flush stays here.
            std::thread windower([&target]
                {
                    Assert::IsFalse(ff_dx12_on_owner_thread());

                    for (size_t i = 1; i <= 50; i++)
                    {
                        ff_dx12_defer_resize_target(&target, 400 + i, 300 + i);
                    }
                });

            windower.join();

            Assert::IsTrue(ff_dx12_has_deferred());
            Assert::IsTrue(ff_dx12_flush_deferred());

            Assert::AreEqual<size_t>(450, ff_dx12_target_window_width(&target));
            Assert::AreEqual<size_t>(350, ff_dx12_target_window_height(&target));
            Assert::IsTrue(ff_dx12_target_window_valid(&target));

            ff_dx12_target_window_destroy(&target);
        }

        TEST_METHOD(concurrent_queuing_from_many_threads_is_safe)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_target_window target{};
            Assert::IsTrue(ff_dx12_target_window_init(&target, create_window()));

            // Hammers the critical section from several threads at once. A missing lock shows up
            // here as a torn size or a corrupted count rather than a clean failure.
            std::vector<std::thread> threads;
            for (int t = 0; t < 4; t++)
            {
                threads.emplace_back([&target]
                    {
                        for (size_t i = 0; i < 500; i++)
                        {
                            ff_dx12_defer_resize_target(&target, 640, 480);
                            ff_dx12_has_deferred();
                        }
                    });
            }

            for (std::thread& t : threads)
            {
                t.join();
            }

            // Every request named the same size and the same target, so exactly one resize is
            // queued no matter how the interleaving went.
            Assert::IsTrue(ff_dx12_flush_deferred());
            Assert::AreEqual<size_t>(640, ff_dx12_target_window_width(&target));
            Assert::IsTrue(ff_dx12_target_window_valid(&target));

            ff_dx12_target_window_destroy(&target);
        }

        TEST_METHOD(owner_thread_is_the_thread_that_called_init)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));
            Assert::IsTrue(ff_dx12_on_owner_thread());

            bool owner_on_other_thread = true;
            std::thread other([&owner_on_other_thread]
                {
                    owner_on_other_thread = ff_dx12_on_owner_thread();
                });
            other.join();

            Assert::IsFalse(owner_on_other_thread);
        }

        TEST_METHOD(ownership_can_be_handed_to_another_thread)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            // This is how the game thread takes over once it starts running.
            bool owner_after_claim = false;
            std::thread game([&owner_after_claim]
                {
                    ff_dx12_set_owner_thread();
                    owner_after_claim = ff_dx12_on_owner_thread();
                });
            game.join();

            Assert::IsTrue(owner_after_claim);
            Assert::IsFalse(ff_dx12_on_owner_thread());

            // Take it back so cleanup's ff_dx12_destroy runs on the owning thread.
            ff_dx12_set_owner_thread();
            Assert::IsTrue(ff_dx12_on_owner_thread());
        }

        TEST_METHOD(queuing_before_init_is_harmless)
        {
            // The window can produce messages before the device exists. Without a queue there is
            // nowhere to put them, and dropping them is correct: init reads the real client size.
            ff_dx12_target_window target{};
            ff_dx12_defer_resize_target(&target, 640, 480);
            ff_dx12_defer_reset_device(false);

            Assert::IsFalse(ff_dx12_has_deferred());
        }

        TEST_METHOD(queue_is_dropped_by_destroy)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_target_window target{};
            Assert::IsTrue(ff_dx12_target_window_init(&target, create_window()));

            ff_dx12_defer_resize_target(&target, 640, 480);
            ff_dx12_target_window_destroy(&target);

            ff_dx12_destroy();

            // A queue that survived destroy would be applied against a dead device on the next
            // init, so it has to be empty here.
            Assert::IsFalse(ff_dx12_has_deferred());

            Assert::IsTrue(ff_dx12_init(nullptr));
            Assert::IsFalse(ff_dx12_has_deferred());
            Assert::IsTrue(ff_dx12_flush_deferred());
        }

        TEST_METHOD(a_failing_resize_does_not_spin_the_flush_loop)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_target_window target{};
            Assert::IsTrue(ff_dx12_target_window_init(&target, create_window()));

            // A resize failure calls ff_dx12_device_fatal_error, which invalidates the device.
            // If anything on that path re-queued work, flush_deferred would never terminate.
            // Destroying the window makes set_size fail on its IsWindow check.
            destroy_window();

            ff_dx12_defer_resize_target(&target, 640, 480);

            // The real assertion is that this returns at all.
            ff_dx12_flush_deferred();
            Assert::IsFalse(ff_dx12_has_deferred());

            ff_dx12_target_window_destroy(&target);
        }

        TEST_METHOD(minimize_queues_a_zero_size_and_stays_valid)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_target_window target{};
            Assert::IsTrue(ff_dx12_target_window_init(&target, create_window()));

            // WM_SIZE on minimize reports a 0x0 client area, which DXGI rejects. set_size clamps
            // to 1x1, so the swap chain has to survive it and come back on restore.
            ff_dx12_defer_resize_target(&target, 0, 0);
            Assert::IsTrue(ff_dx12_flush_deferred());

            Assert::IsTrue(ff_dx12_target_window_valid(&target));
            Assert::AreEqual<size_t>(1, ff_dx12_target_window_width(&target));
            Assert::AreEqual<size_t>(1, ff_dx12_target_window_height(&target));

            ff_dx12_defer_resize_target(&target, 320, 240);
            Assert::IsTrue(ff_dx12_flush_deferred());

            Assert::IsTrue(ff_dx12_target_window_valid(&target));
            Assert::AreEqual<size_t>(320, ff_dx12_target_window_width(&target));

            ff_dx12_target_window_destroy(&target);
        }

        TEST_METHOD(cancel_is_safe_for_a_target_that_was_never_queued)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_target_window never_queued{};
            ff_dx12_cancel_deferred_target(&never_queued);
            Assert::IsFalse(ff_dx12_has_deferred());

            // Also safe twice, since destroy can run after an explicit cancel.
            ff_dx12_target_window target{};
            ff_dx12_defer_resize_target(&target, 640, 480);
            ff_dx12_cancel_deferred_target(&target);
            ff_dx12_cancel_deferred_target(&target);
            Assert::IsFalse(ff_dx12_has_deferred());
        }

        TEST_METHOD(cancel_keeps_the_remaining_entries_intact)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_target_window target{};
            Assert::IsTrue(ff_dx12_target_window_init(&target, create_window()));

            // cancel removes by swapping in the last entry, so cancelling a middle entry is the
            // case that can lose or duplicate a neighbour.
            ff_dx12_target_window first{};
            ff_dx12_target_window middle{};

            ff_dx12_defer_resize_target(&first, 100, 100);
            ff_dx12_defer_resize_target(&middle, 200, 200);
            ff_dx12_defer_resize_target(&target, 640, 480);

            ff_dx12_cancel_deferred_target(&middle);
            ff_dx12_cancel_deferred_target(&first);

            Assert::IsTrue(ff_dx12_has_deferred());
            Assert::IsTrue(ff_dx12_flush_deferred());

            // The surviving entry is the real one, so it must still be applied.
            Assert::AreEqual<size_t>(640, ff_dx12_target_window_width(&target));

            ff_dx12_target_window_destroy(&target);
        }

        TEST_METHOD(force_is_sticky_across_queued_resets)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            const uint64_t before = ff_dx12_device_reset_count();

            // A forced reset must not be downgraded by a later non-forced request, or a caller
            // that knows the device is broken would be overruled by routine polling.
            ff_dx12_defer_reset_device(true);
            ff_dx12_defer_reset_device(false);

            Assert::IsTrue(ff_dx12_flush_deferred());
            Assert::AreEqual(before + 1, ff_dx12_device_reset_count());
        }

        TEST_METHOD(unforced_reset_of_a_healthy_device_is_a_no_op)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            const uint64_t before = ff_dx12_device_reset_count();

            // Cheap enough to queue speculatively, which is what makes it safe to call from a
            // window message without knowing whether anything is actually wrong.
            ff_dx12_defer_reset_device(false);
            Assert::IsTrue(ff_dx12_flush_deferred());

            Assert::AreEqual(before, ff_dx12_device_reset_count());
            Assert::IsTrue(ff_dx12_device_valid());
        }

    private:
        HWND hwnd = nullptr;
    };
}
