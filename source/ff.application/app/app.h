#pragma once

namespace ff
{
    struct init_app_params;
    class init_dx_async;

    enum class app_update_t
    {
        running,
        single_step,
        stopped,
    };

    struct app_time_t
    {
        size_t frame_count{};
        size_t update_count{};
        int64_t perf_clock_ticks{};
        double clock_seconds{};
        double update_seconds{};
        double unused_update_seconds{};
        double time_scale{ 1.0 };
        ff::app_update_t update_type{ ff::app_update_t::running };
    };

    const ff::module_version_t& app_version();
    const ff::app_time_t& app_time();
    const ff::window& app_window();
    bool app_window_active();

    std::filesystem::path app_roaming_path();
    std::filesystem::path app_local_path();
    std::filesystem::path app_temp_path();
}

namespace ff::internal::app
{
    bool init(const ff::init_app_params& params, const ff::init_dx_async& init_dx);
    void destroy();

    ff::resource_object_provider& app_resources();
}
