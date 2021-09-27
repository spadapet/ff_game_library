#include "pch.h"

namespace ff::test::base
{
    TEST_CLASS(uuid_tests)
    {
    public:
        TEST_METHOD(construct)
        {
            ff::uuid id_null = ff::uuid::null();
            ff::uuid id_null2 = GUID_NULL;
            Assert::IsTrue(id_null == id_null2);

            ff::uuid id_std_marshal = "{00000017-0000-0000-c000-000000000046}"sv;
            ff::uuid id_std_marshal2 = CLSID_StdMarshal;
            Assert::IsTrue(id_std_marshal == id_std_marshal2);
            Assert::AreEqual("{00000017-0000-0000-C000-000000000046}"s, id_std_marshal2.to_string());

            Assert::IsTrue(id_std_marshal == id_std_marshal2);
            Assert::IsTrue(id_std_marshal != id_null);
            Assert::IsTrue(id_std_marshal > id_null);
            Assert::IsTrue(id_std_marshal >= id_null);
            Assert::IsTrue(id_null < id_std_marshal);
            Assert::IsTrue(id_null <= id_std_marshal);
        }
    };
}
