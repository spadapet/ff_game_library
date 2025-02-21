#pragma once

#include "../graphics/dxgi/target_window_base.h"
#include "../input/input_mapping.h"

namespace ff
{
    enum class app_update_t;
}

namespace ff::internal
{
    class debug_stats
    {
    public:
        debug_stats(
            std::shared_ptr<ff::dxgi::target_window_base> app_target,
            std::shared_ptr<ff::resource_object_provider> app_resources,
            const ff::perf_results& perf_results);

        void update_input();
        void frame_started(ff::app_update_t type);
        void render(ff::app_update_t type, ff::dxgi::command_context_base& context, ff::dxgi::target_base& target);

    private:
        std::shared_ptr<ff::dxgi::target_window_base> app_target;
        const ff::perf_results& perf_results;
    };
}
