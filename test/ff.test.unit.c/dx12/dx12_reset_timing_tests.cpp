#include "pch.h"

namespace ff::test::dx12
{
    // Not a correctness test: measures how long ID3D12CommandAllocator::Reset plus
    // ID3D12GraphicsCommandList::Reset take on the calling thread, which is what decides whether
    // the reset has to be moved off the render thread like the old C++ code did.
    TEST_CLASS(dx12_reset_timing_tests)
    {
    public:
        TEST_METHOD_CLEANUP(cleanup)
        {
            ff_dx12_destroy();
        }

        TEST_METHOD(measure_command_list_reset_cost)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_queue* queue = ff_dx12_direct_queue();
            Assert::IsNotNull(queue);

            uint8_t data[1024];
            memset(data, 0x19, sizeof(data));

            ff_dx12_buffer buffer{};
            Assert::IsTrue(ff_dx12_buffer_init_gpu(&buffer, ff_dx12_buffer_type_vertex, 0));

            LARGE_INTEGER freq{};
            QueryPerformanceFrequency(&freq);

            const size_t warmup = 16;
            const size_t iterations = 256;
            double total_us = 0.0;
            double worst_us = 0.0;

            for (size_t i = 0; i < warmup + iterations; i++)
            {
                ff_dx12_commands commands{};
                Assert::IsTrue(ff_dx12_queue_new_commands(queue, &commands));
                Assert::IsTrue(ff_dx12_buffer_update(&buffer, &commands, data, sizeof(data)));

                LARGE_INTEGER start{};
                QueryPerformanceCounter(&start);

                ff_dx12_queue_execute(queue, &commands);

                LARGE_INTEGER stop{};
                QueryPerformanceCounter(&stop);

                if (i >= warmup)
                {
                    const double us = (double)(stop.QuadPart - start.QuadPart) * 1000000.0 / (double)freq.QuadPart;
                    total_us += us;
                    worst_us = (us > worst_us) ? us : worst_us;
                }

                ff_dx12_frame_complete();
            }

            ff_dx12_wait_for_idle();
            ff_dx12_buffer_destroy(&buffer);

            wchar_t message[256];
            _snwprintf_s(message, _countof(message), _TRUNCATE,
                L"execute+reset: avg %.1f us, worst %.1f us over %zu iterations",
                total_us / (double)iterations, worst_us, iterations);
            Logger::WriteMessage(message);
        }
    };
}
