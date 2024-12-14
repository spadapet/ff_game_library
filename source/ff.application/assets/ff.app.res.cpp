#include "pch.h"
#include "ff.app.res.h"

namespace ff
{
    std::shared_ptr<::ff::data_base> assets_app_data()
    {
        return ::assets::app::data();
    }
}
