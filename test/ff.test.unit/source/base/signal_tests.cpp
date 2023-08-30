#include "pch.h"

namespace ff::test::base
{
    TEST_CLASS(signal_tests)
    {
    public:
        TEST_METHOD(void_args)
        {
            ff::signal<> sig;
            int i = 0;

            ff::signal_connection c1 = sig.connect([&i]()
                {
                    i++;
                });

            ff::signal_connection c2 = sig.connect([&i]()
                {
                    i += 100;
                });

            ff::signal_connection c3;
            c3 = sig.connect([&i, &c3]()
                {
                    i += 1000;
                    c3.disconnect();
                });

            sig.notify();
            sig.notify();

            Assert::AreEqual(1202, i);

            c1.disconnect();
            sig.notify();
            Assert::AreEqual(1302, i);

            c2.disconnect();
            sig.notify();
            Assert::AreEqual(1302, i);
        }

        TEST_METHOD(simple_args)
        {
            ff::signal<int> sig;
            int i = 0;

            ff::signal_connection c1 = sig.connect([&i](int a)
                {
                    i += a;
                });

            sig.notify(100);
            sig.notify(50);

            Assert::AreEqual(150, i);
        }

        TEST_METHOD(connection_outlives_signal)
        {
            auto sig = std::make_unique<ff::signal<int>>();
            int i = 0;

            ff::signal_connection c1 = sig->connect([&i](int a)
                {
                    i += a;
                });

            Assert::IsTrue(c1);

            sig->notify(100);
            sig->notify(50);
            sig.reset();

            Assert::IsFalse(c1);
            Assert::AreEqual(150, i);
        }

        TEST_METHOD(reuse_disconnected_entry)
        {
            ff::signal<> sig;
            int i = 0;

            ff::signal_connection c1 = sig.connect([&i]() { i += 10; });
            ff::signal_connection c2 = sig.connect([&i]() { i += 100; });
            c1 = sig.connect([&i]() { i += 1000; });
            c2 = sig.connect([&i]() { i += 10000; });

            sig.notify();

            Assert::AreEqual(11000, i);
        }
    };
}
