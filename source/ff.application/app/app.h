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

    std::filesystem::path app_roaming_path();
    std::filesystem::path app_local_path();
    std::filesystem::path app_temp_path();

    ff::dxgi::target_window_base& app_render_target();
}

namespace ff::internal::app
{
    bool init(ff::window* window, const ff::init_app_params& params);
    void destroy();
    ff::resource_object_provider& app_resources();

    using namespace std::string_view_literals;
    constexpr std::string_view xaml_assembly_name = "ff.application.xaml"sv;
    constexpr std::string_view xaml_pack_uri = "pack://application:,,,/ff.application.xaml;component/"sv;
}
