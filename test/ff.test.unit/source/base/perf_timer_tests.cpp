#include "pch.h"

namespace ff::test::base
{
    TEST_CLASS(perf_timer_tests)
    {
    public:
        TEST_METHOD(counters)
        {
            ff::perf_measures measures;
            ff::perf_counter c1(measures, "Counter 1");
            ff::perf_counter c2(measures, "Counter 2");

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

                    measures.enabled(false);

                    // Nest 3, disabled
                    {
                        ff::perf_timer t2(c2);
                        std::this_thread::sleep_for(1s);
                    }
                }
            }

            Assert::IsFalse(measures.enabled());
            Assert::IsNotNull(measures.first());

            for (const ff::perf_counter_entry* entry = measures.first(); entry; entry = entry->next)
            {
                ff::log::write(ff::log::type::test, ff::string::indent_string(entry->level * 2),
                    "Counter:", entry->counter->name,
                    ", Ticks:", entry->ticks,
                    ", Count:", entry->count);
            }

            if (!::IsDebuggerPresent())
            {
                double percent = static_cast<double>(measures.first()->next->ticks) / static_cast<double>(measures.first()->ticks);
                Assert::IsTrue(percent > 0.58 && percent < 0.62);
            }
        }
    };
}
