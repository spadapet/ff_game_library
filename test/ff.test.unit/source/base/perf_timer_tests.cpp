#include "pch.h"

namespace ff::test::base
{
    TEST_CLASS(perf_timer_tests)
    {
    public:
        TEST_METHOD(counters)
        {
            ff::perf_measures measures;
            ff::perf_results results{};
            ff::perf_counter c1(measures, "Counter 1");
            ff::perf_counter c2(measures, "Counter 2");

            measures.reset(1.0);

            // Nested timers
            {
                ff::perf_timer t1(c1);
                std::this_thread::sleep_for(1s);
                {
                    // Nest 1
                    {
                        ff::perf_timer t2(c2);
                        std::this_thread::sleep_for(2s);
                    }

                    // Nest 2
                    {
                        ff::perf_timer t2(c2);
                        std::this_thread::sleep_for(1s);
                    }

                    // Nest 3
                    {
                        ff::perf_timer t2(c2);
                        std::this_thread::sleep_for(1s);
                    }
                }
            }

            measures.reset(6.0, &results, true);

            Assert::AreEqual(5.0, results.delta_seconds);
            Assert::AreEqual<size_t>(2, results.counter_infos.size());

            for (const ff::perf_results::counter_info& info : results.counter_infos)
            {
                ff::log::write(ff::log::type::test, ff::string::indent_string(info.level * 2),
                    "Counter:", info.counter->name,
                    ", Ticks:", info.ticks,
                    ", Count:", info.hit_last_frame);
            }

            if (!::IsDebuggerPresent())
            {
                double percent = static_cast<double>(results.counter_infos[1].ticks) / results.counter_infos[0].ticks;
                Assert::IsTrue(percent > 0.78 && percent < 0.82);
            }
        }
    };
}
