#pragma once

namespace ff
{
    struct init_app_params;

    struct app_time_t
    {
        size_t frame_count{};
        size_t advance_count{};
        int64_t perf_clock_ticks{};
        double clock_seconds{};
        double advance_seconds{};
        double unused_advance_seconds{};
        double time_scale{ 1.0 };
    };

    const std::string& app_product_name();
    const std::string& app_internal_name();
    const ff::app_time_t& app_time();
    const ff::window& app_window();
    bool app_window_active();

    std::filesystem::path app_roaming_path();
    std::filesystem::path app_local_path();
    std::filesystem::path app_temp_path();
}

namespace ff::internal::app
{
    bool init(const ff::init_app_params& params, ff::init_dx& async_init_dx);
    void destroy();

    ff::resource_object_provider& app_resources();
}
