#pragma once

namespace ff
{
    struct app_time_t;
    struct frame_time_t;
    struct init_app_params;

    const std::string& app_name();
    const ff::app_time_t& app_time();
    const ff::frame_time_t& frame_time();
}

namespace ff::internal::app
{
    bool init(const ff::init_app_params& params);
    void destroy();
}
