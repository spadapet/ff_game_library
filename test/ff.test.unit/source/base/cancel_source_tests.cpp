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
            Assert::IsTrue(token.wait_handle());
            Assert::IsFalse(ff::is_event_set(token.wait_handle()));
        }

        TEST_METHOD(valid_token)
        {
            ff::cancel_source source;
            ff::cancel_token token = source.token();
            Assert::IsTrue(token.valid());
            Assert::IsFalse(token.canceled());
            Assert::IsTrue(token.wait_handle());
            Assert::IsFalse(ff::is_event_set(token.wait_handle()));

            source.cancel();
            Assert::IsTrue(token.canceled());
            Assert::IsTrue(ff::is_event_set(token.wait_handle()));
            Assert::ExpectException<ff::cancel_exception>([&token]() { token.throw_if_canceled(); });
        }

        TEST_METHOD(cancel_other_thread)
        {
            ff::cancel_source source;
            ff::win_handle cancel_event[3] = { ff::win_handle::create_event(), ff::win_handle::create_event(), ff::win_handle::create_event() };
            int i[3] = {};

            source.token().connect([&i, &cancel_event]()
                {
                    i[0] = 10;
                    ::SetEvent(cancel_event[0]);
                });

            ff::cancel_connection connection0 = source.token().connect([&i, &cancel_event]()
                {
                    i[1] = 20;
                    ::SetEvent(cancel_event[1]);
                });
            {
                ff::cancel_connection connection1 = source.token().connect([&i, &cancel_event]()
                    {
                        i[2] = 30;
                        ::SetEvent(cancel_event[2]);
                    });
            }

            ff::thread_pool::add_task([source]()
                {
                    ::Sleep(1000);
                    source.cancel();
                });

            std::array<HANDLE, 2> handles1{ source.token().wait_handle(), cancel_event[1]};
            bool success1 = ff::wait_for_all_handles(handles1.data(), handles1.size(), 2000);
            Assert::IsTrue(success1);

            std::array<HANDLE, 2> handles2{ cancel_event[0], cancel_event[2] };
            bool success2 = ff::wait_for_all_handles(handles2.data(), handles2.size(), 1000);
            Assert::IsFalse(success2);

            Assert::AreEqual(0, i[0]);
            Assert::AreEqual(20, i[1]);
            Assert::AreEqual(0, i[2]);
        }
    };
}
