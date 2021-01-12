#include "pch.h"
#include "source/resource.h"

namespace entity_test
{
    TEST_CLASS(entity_test)
    {
    public:
        TEST_METHOD(set_component)
        {
            ff::entity_domain domain;

            ff::entity entity = domain.create();
        }
    };
}
