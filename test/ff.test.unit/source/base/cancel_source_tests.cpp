#include "pch.h"

namespace ff::test::base
{
    TEST_CLASS(cancel_source_tests)
    {
    public:
        TEST_METHOD(invalid_token)
        {
            ff::cancel_token token;
            Assert::IsFalse(token.valid());
            Assert::IsFalse(token.canceled());
            Assert::IsNotNull(token.wait_handle());
            Assert::IsFalse(ff::is_event_set(token.wait_handle()));
        }

        TEST_METHOD(valid_token)
        {
            ff::cancel_source source;
            ff::cancel_token token = source.token();
            Assert::IsTrue(token.valid());
            Assert::IsFalse(token.canceled());
            Assert::IsNotNull(token.wait_handle());
            Assert::IsFalse(ff::is_event_set(token.wait_handle()));

            source.cancel();
            Assert::IsTrue(token.canceled());
            Assert::IsTrue(ff::is_event_set(token.wait_handle()));
            Assert::ExpectException<ff::cancel_exception>([&token]() { token.throw_if_canceled(); });
        }

        TEST_METHOD(cancel_other_thread)
        {
            ff::cancel_source source;
            int i = 0;

            source.token().notify([&i]()
                {
                    i = 10;
                });

            ff::thread_pool::get()->add_task([source]()
                {
                    ::Sleep(1000);
                    source.cancel();
                });

            bool success = ff::wait_for_handle(source.token().wait_handle(), 2000);
            Assert::IsTrue(success);
            Assert::AreEqual(10, i);
        }
    };
}
