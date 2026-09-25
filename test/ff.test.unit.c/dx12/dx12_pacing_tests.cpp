#include "pch.h"

namespace ff::test::base
{
    TEST_CLASS(dx12_pacing_tests)
    {
    public:
        static const double refresh;

        static void add_frames(ff_dx12_pacing* pacing, size_t count, double frame_seconds)
        {
            for (size_t i = 0; i < count; i++)
            {
                internal_ff_dx12_pacing_add_frame(pacing, frame_seconds);
            }
        }

        // Enough frames to get past the post-change skip window and complete whole windows.
        static void add_windows(ff_dx12_pacing* pacing, size_t windows, double frame_seconds)
        {
            add_frames(pacing, windows * FF_DX12_PACING_WINDOW_FRAMES, frame_seconds);
        }

        // Feeds frames until the next window boundary, so a following burst lands inside a single
        // window instead of straddling two.
        static void align_to_window(ff_dx12_pacing* pacing, double frame_seconds)
        {
            while (pacing->skip_frames || pacing->window_frames)
            {
                internal_ff_dx12_pacing_add_frame(pacing, frame_seconds);
            }
        }

        TEST_METHOD(a_single_bad_window_does_not_demote)
        {
            ff_dx12_pacing pacing;
            internal_ff_dx12_pacing_init(&pacing, refresh);

            add_windows(&pacing, 4, refresh);
            align_to_window(&pacing, refresh);

            // One window full of late frames, then back to healthy. Demoting costs vsync, so it
            // must take a sustained problem rather than one unlucky window.
            add_frames(&pacing, FF_DX12_PACING_WINDOW_FRAMES, refresh * 3.0);
            add_windows(&pacing, 4, refresh);

            Assert::AreEqual((size_t)0, pacing.stage);
            Assert::AreEqual((uint64_t)0, pacing.stage_changes);
        }

        // Two windows in a row over budget is a real problem, not noise, and must demote.
        TEST_METHOD(two_bad_windows_in_a_row_demote)
        {
            ff_dx12_pacing pacing;
            internal_ff_dx12_pacing_init(&pacing, refresh);

            add_windows(&pacing, 4, refresh);
            align_to_window(&pacing, refresh);

            add_frames(&pacing, FF_DX12_PACING_WINDOW_FRAMES * 2, refresh * 3.0);

            Assert::AreEqual((size_t)1, pacing.stage);
        }

        TEST_METHOD(starts_at_best_stage)
        {
            ff_dx12_pacing pacing;
            internal_ff_dx12_pacing_init(&pacing, refresh);

            Assert::AreEqual((size_t)0, pacing.stage);
            Assert::AreEqual((uint32_t)1, internal_ff_dx12_pacing_latency(&pacing));
            Assert::IsTrue(internal_ff_dx12_pacing_vsync(&pacing));
        }

        TEST_METHOD(steady_good_frames_never_leave_stage_zero)
        {
            ff_dx12_pacing pacing;
            internal_ff_dx12_pacing_init(&pacing, refresh);

            add_windows(&pacing, 100, refresh);

            Assert::AreEqual((size_t)0, pacing.stage);
            Assert::AreEqual((uint64_t)0, pacing.total_late_frames);
            Assert::AreEqual((uint64_t)0, pacing.stage_changes);
        }

        // The whole point of the ladder for an action game: occasional hitches must not cost
        // latency or vsync, because dropping either cannot fix an OS scheduling stall anyway.
        TEST_METHOD(occasional_late_frames_are_tolerated)
        {
            ff_dx12_pacing pacing;
            internal_ff_dx12_pacing_init(&pacing, refresh);

            for (size_t window = 0; window < 50; window++)
            {
                add_frames(&pacing, 2, refresh * 3.0);
                add_frames(&pacing, FF_DX12_PACING_WINDOW_FRAMES - 2, refresh);
            }

            Assert::AreEqual((size_t)0, pacing.stage);
            Assert::AreEqual((uint32_t)1, internal_ff_dx12_pacing_latency(&pacing));
            Assert::IsTrue(internal_ff_dx12_pacing_vsync(&pacing));
        }

        TEST_METHOD(sustained_slow_frames_demote)
        {
            ff_dx12_pacing pacing;
            internal_ff_dx12_pacing_init(&pacing, refresh);

            add_windows(&pacing, 4, refresh * 3.0);

            Assert::IsTrue(pacing.stage > 0);
        }

        // Drives exactly enough frames to trigger the first stage change: the post-init skip
        // window, then one full evaluation window of late frames.
        static void add_frames_to_first_demotion(ff_dx12_pacing* pacing, double frame_seconds)
        {
            const uint64_t changes = pacing->stage_changes;

            for (size_t i = 0; i < 1000 && pacing->stage_changes == changes; i++)
            {
                internal_ff_dx12_pacing_add_frame(pacing, frame_seconds);
            }
        }

        TEST_METHOD(demotion_gives_up_vsync_before_latency)
        {
            ff_dx12_pacing pacing;
            internal_ff_dx12_pacing_init(&pacing, refresh);

            add_frames_to_first_demotion(&pacing, refresh * 3.0);

            Assert::AreEqual((size_t)1, pacing.stage);
            Assert::AreEqual((uint32_t)1, internal_ff_dx12_pacing_latency(&pacing));
            Assert::IsFalse(internal_ff_dx12_pacing_vsync(&pacing));
        }

        TEST_METHOD(demotion_stops_at_the_last_stage)
        {
            ff_dx12_pacing pacing;
            internal_ff_dx12_pacing_init(&pacing, refresh);

            add_windows(&pacing, 500, refresh * 10.0);

            Assert::AreEqual((size_t)FF_DX12_PACING_STAGE_COUNT - 1, pacing.stage);
        }

        TEST_METHOD(recovery_returns_all_the_way_to_stage_zero)
        {
            ff_dx12_pacing pacing;
            internal_ff_dx12_pacing_init(&pacing, refresh);

            add_windows(&pacing, 20, refresh * 10.0);
            Assert::AreEqual((size_t)FF_DX12_PACING_STAGE_COUNT - 1, pacing.stage);

            add_windows(&pacing, 200, refresh);

            Assert::AreEqual((size_t)0, pacing.stage);
            Assert::AreEqual((uint32_t)1, internal_ff_dx12_pacing_latency(&pacing));
            Assert::IsTrue(internal_ff_dx12_pacing_vsync(&pacing));
        }

        // The bug found in the Release benchmark: a stale slow average carried into the new stage
        // kept re-triggering the demote test, so the ladder could never climb back down.
        TEST_METHOD(average_is_not_carried_across_a_stage_change)
        {
            ff_dx12_pacing pacing;
            internal_ff_dx12_pacing_init(&pacing, refresh);

            add_frames_to_first_demotion(&pacing, refresh * 3.0);
            Assert::IsTrue(pacing.stage > 0);

            // Reseeded to the refresh interval, not left holding the ~50ms that caused the demote.
            Assert::AreEqual(refresh, pacing.average_seconds, 0.0001);
        }

        // A machine that genuinely cannot hold a stage must settle instead of flapping between two
        // stages forever, since every flap is a visible latency and tearing change.
        TEST_METHOD(alternating_load_does_not_oscillate_forever)
        {
            ff_dx12_pacing pacing;
            internal_ff_dx12_pacing_init(&pacing, refresh);

            uint64_t changes_early = 0;

            for (size_t round = 0; round < 60; round++)
            {
                add_windows(&pacing, 3, refresh * 3.0);
                add_windows(&pacing, 3, refresh);

                if (round == 9)
                {
                    changes_early = pacing.stage_changes;
                }
            }

            const uint64_t changes_late = pacing.stage_changes - changes_early;

            // The back-off must make later flapping strictly rarer than the initial burst.
            Assert::IsTrue(changes_late < changes_early,
                L"stage changes did not slow down as the back-off grew");
        }

        TEST_METHOD(promotion_requires_more_evidence_after_a_failed_attempt)
        {
            ff_dx12_pacing pacing;
            internal_ff_dx12_pacing_init(&pacing, refresh);

            const size_t promote_at_start = pacing.promote_windows;

            add_frames_to_first_demotion(&pacing, refresh * 3.0);

            Assert::IsTrue(pacing.promote_windows > promote_at_start);
        }

        TEST_METHOD(promote_back_off_is_bounded)
        {
            ff_dx12_pacing pacing;
            internal_ff_dx12_pacing_init(&pacing, refresh);

            for (size_t round = 0; round < 200; round++)
            {
                add_windows(&pacing, 3, refresh * 3.0);
                add_windows(&pacing, 3, refresh);
            }

            Assert::IsTrue(pacing.promote_windows <= 64);
        }

        TEST_METHOD(interrupt_keeps_the_stage_but_clears_the_window)
        {
            ff_dx12_pacing pacing;
            internal_ff_dx12_pacing_init(&pacing, refresh);

            add_windows(&pacing, 4, refresh * 3.0);
            const size_t stage_before = pacing.stage;

            internal_ff_dx12_pacing_interrupt(&pacing);

            Assert::AreEqual(stage_before, pacing.stage);
            Assert::AreEqual((size_t)0, pacing.window_frames);
            Assert::AreEqual((size_t)0, pacing.window_late_frames);
            Assert::AreEqual((int64_t)0, pacing.last_tick);
        }

        // A long stall right after a resize or reset must not be charged to the stage.
        TEST_METHOD(frames_right_after_an_interrupt_are_ignored)
        {
            ff_dx12_pacing pacing;
            internal_ff_dx12_pacing_init(&pacing, refresh);

            add_windows(&pacing, 100, refresh);
            Assert::AreEqual((size_t)0, pacing.stage);

            internal_ff_dx12_pacing_interrupt(&pacing);

            add_frames(&pacing, FF_DX12_PACING_WINDOW_FRAMES / 2, refresh * 20.0);
            add_windows(&pacing, 4, refresh);

            Assert::AreEqual((size_t)0, pacing.stage);
        }

        TEST_METHOD(a_high_refresh_display_judges_lateness_against_its_own_rate)
        {
            const double fast_refresh = 1.0 / 144.0;

            ff_dx12_pacing pacing;
            internal_ff_dx12_pacing_init(&pacing, fast_refresh);

            // Comfortably fast for 60Hz, far too slow for 144Hz.
            add_windows(&pacing, 4, 1.0 / 60.0);

            Assert::IsTrue(pacing.stage > 0);
            Assert::IsTrue(pacing.total_late_frames > 0);
        }

        TEST_METHOD(a_sixty_hertz_display_is_happy_at_sixty_fps)
        {
            ff_dx12_pacing pacing;
            internal_ff_dx12_pacing_init(&pacing, 1.0 / 60.0);

            add_windows(&pacing, 50, 1.0 / 60.0);

            Assert::AreEqual((size_t)0, pacing.stage);
            Assert::AreEqual((uint64_t)0, pacing.total_late_frames);
        }

        TEST_METHOD(zero_and_negative_frame_times_are_ignored)
        {
            ff_dx12_pacing pacing;
            internal_ff_dx12_pacing_init(&pacing, refresh);

            add_windows(&pacing, 100, refresh);

            const double average_before = pacing.average_seconds;

            internal_ff_dx12_pacing_add_frame(&pacing, 0.0);
            internal_ff_dx12_pacing_add_frame(&pacing, -1.0);

            Assert::AreEqual(average_before, pacing.average_seconds, 0.0000001);
            Assert::AreEqual((size_t)0, pacing.stage);
        }

        TEST_METHOD(an_invalid_refresh_rate_falls_back_to_sixty)
        {
            ff_dx12_pacing pacing;
            internal_ff_dx12_pacing_init(&pacing, 0.0);

            Assert::AreEqual(1.0 / 60.0, pacing.refresh_seconds, 0.0000001);
        }

        TEST_METHOD(jitter_around_the_refresh_interval_is_not_late)
        {
            ff_dx12_pacing pacing;
            internal_ff_dx12_pacing_init(&pacing, refresh);

            for (size_t i = 0; i < 50 * FF_DX12_PACING_WINDOW_FRAMES; i++)
            {
                add_frames(&pacing, 1, refresh * ((i % 2) ? 1.05 : 0.95));
            }

            Assert::AreEqual((uint64_t)0, pacing.total_late_frames);
            Assert::AreEqual((size_t)0, pacing.stage);
        }

        TEST_METHOD(latency_never_exceeds_two_frames)
        {
            ff_dx12_pacing pacing;
            internal_ff_dx12_pacing_init(&pacing, refresh);

            for (size_t stage = 0; stage < FF_DX12_PACING_STAGE_COUNT; stage++)
            {
                pacing.stage = stage;
                Assert::IsTrue(internal_ff_dx12_pacing_latency(&pacing) <= 2);
                Assert::IsTrue(internal_ff_dx12_pacing_latency(&pacing) >= 1);
            }
        }
    };

    const double dx12_pacing_tests::refresh = 1.0 / 60.0;
}
