#pragma once

namespace ff
{
    struct app_time_t;
    struct frame_time_t;
    struct init_app_params;

    const std::string& app_name();
    const ff::app_time_t& app_time();
    const ff::frame_time_t& frame_time();
    ff::dx11_target_window& app_render_target();
    void app_measure_startup_perf();
}

namespace ff::internal::app
{
    bool init(const ff::init_app_params& params);
    void destroy();
}
