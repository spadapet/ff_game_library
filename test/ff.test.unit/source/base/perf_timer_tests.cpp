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

            measures.enabled(true);
            Assert::IsNull(measures.first());
        }
    };
}
