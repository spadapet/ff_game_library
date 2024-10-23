#pragma once

namespace ff
{
    struct init_app_params;

    struct app_time_t
    {
        size_t frame_count;
        size_t advance_count;
        int64_t perf_clock_ticks;
        double clock_seconds;
        double advance_seconds;
        double unused_advance_seconds;
        double time_scale;
    };

    const std::string& app_product_name();
    const std::string& app_internal_name();
    const ff::app_time_t& app_time();

    ff::dxgi::target_window_base& app_render_target();
}

namespace ff::internal::app
{
    bool init(const ff::init_app_params& params);
    void destroy();
    ff::resource_object_provider& app_resources();

    using namespace std::string_view_literals;
    static constexpr std::string_view xaml_assembly_name = "ff.application.xaml"sv;
    static constexpr std::string_view xaml_pack_uri = "pack://application:,,,/ff.application.xaml;component/"sv;
}
