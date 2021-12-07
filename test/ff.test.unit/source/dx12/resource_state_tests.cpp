#include "pch.h"
#include "test_base.h"

namespace ff::test::dx12
{
    TEST_CLASS(resource_state_tests), public ff::test::dx12::test_base
    {
    public:
        TEST_METHOD_INITIALIZE(initialize)
        {
            this->test_initialize();
        }

        TEST_METHOD_CLEANUP(cleanup)
        {
            this->test_cleanup();
        }

        TEST_METHOD(resource_state_simple)
        {
            ff::dx12::resource_state::state_t state1{ D3D12_RESOURCE_STATE_COMMON, ff::dx12::resource_state::type_t::barrier };
            ff::dx12::resource_state::state_t state2{ D3D12_RESOURCE_STATE_COPY_DEST, ff::dx12::resource_state::type_t::barrier };
            ff::dx12::resource_state rs1(state1.first, state1.second, 2, 4);
            ff::dx12::resource_state rs2(D3D12_RESOURCE_STATE_COPY_SOURCE, ff::dx12::resource_state::type_t::none, 2, 4);

            Assert::IsTrue(rs1.all_same());
            Assert::IsTrue(state1 == rs1.get(0));
            Assert::IsTrue(state1 == rs1.get(1, 1));
            Assert::IsTrue(state1 == rs1.get(0, 2));
            Assert::IsTrue(state1 == rs1.get(1, 3));

            rs1.set(state1.first, state1.second, 1, 1, 1, 2);
            Assert::IsTrue(rs1.all_same());

            rs1.set(state2.first, state2.second, 1, 1, 1, 2);
            Assert::IsFalse(rs1.all_same());

            Assert::IsTrue(state1 == rs1.get(0));
            Assert::IsTrue(state2 == rs1.get(1, 1));
            Assert::IsTrue(state2 == rs1.get(1, 2));

            rs1.set(state1.first, state2.second, 1, 1, 1, 2);
            Assert::IsTrue(rs1.all_same());

            rs1.merge(rs2);
            Assert::IsTrue(rs1.all_same());
            Assert::IsTrue(rs2.all_same());

            rs2.set(D3D12_RESOURCE_STATE_COPY_SOURCE, ff::dx12::resource_state::type_t::barrier, 0, 1, 0, 1);
            Assert::IsFalse(rs2.all_same());

            rs1.merge(rs2);
            Assert::IsFalse(rs1.all_same());
            Assert::IsTrue(rs1.get(0) == rs2.get(0));
            Assert::IsTrue(state1 == rs1.get(0, 1));
        }
    };
}
