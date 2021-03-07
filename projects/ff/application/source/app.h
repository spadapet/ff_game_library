#pragma once

namespace ff
{
    struct init_app_params;
}

namespace ff::internal::app
{
    bool init(const ff::init_app_params& params);
    void destroy();
}
