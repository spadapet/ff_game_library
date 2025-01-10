#include "pch.h"
#include "ff.dx12.res.h"

namespace ff
{
    std::shared_ptr<::ff::data_base> assets_dx12_data()
    {
        return ::assets::dx12::data();
    }
}
