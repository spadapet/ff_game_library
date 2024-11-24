#include "pch.h"
#include "app/app.h"
#include "app/debug_state.h"
#include "ff.app.res.id.h"

using namespace std::string_view_literals;

constexpr size_t CHART_WIDTH = 125;
constexpr uint32_t CHART_WIDTH_U = static_cast<uint32_t>(::CHART_WIDTH);
constexpr uint16_t CHART_WIDTH_U16 = static_cast<uint16_t>(::CHART_WIDTH);
constexpr float CHART_WIDTH_F = static_cast<float>(::CHART_WIDTH);
constexpr float CHART_HEIGHT_F = 64.0f;
static const size_t EVENT_TOGGLE_DEBUG = ff::stable_hash_func("toggle_debug"sv);
static const size_t EVENT_CUSTOM = ff::stable_hash_func("custom_debug"sv);
static std::string_view NONE_NAME = "None"sv;
static ff::internal::debug_view_model* global_debug_view_model{};
static ff::signal<> custom_debug_signal;

ff::internal::debug_page_model::debug_page_model()
    : ff::internal::debug_page_model(""sv, []() { return std::make_shared<ff::state>(); })
{}

ff::internal::debug_page_model::debug_page_model(std::string_view name, std::function<std::shared_ptr<ff::state>()>&& factory)
    : name_(name)
    , factory(std::move(factory))
{}

std::string_view ff::internal::debug_page_model::name() const
{
    return this->is_none() ? ::NONE_NAME : this->name_;
}

const char* ff::internal::debug_page_model::name_cstr() const
{
    return this->name_.c_str();
}

ff::state* ff::internal::debug_page_model::state()
{
    if (!this->state_ && this->factory)
    {
        std::function<std::shared_ptr<ff::state>()> factory = std::move(this->factory);
        this->state_ = factory();
    }

    return this->state_.get();
}

bool ff::internal::debug_page_model::is_none() const
{
    return this->name_.empty();
}

ff::internal::debug_timer_model::debug_timer_model(const ff::perf_results& results, const ff::perf_results::counter_info& info)
    : ff::internal::debug_timer_model::debug_timer_model()
{
    this->info(results, info);
}

void ff::internal::debug_timer_model::info(const ff::perf_results& results, const ff::perf_results::counter_info& info)
{
    const double time_ms = results.delta_ticks ? (info.ticks * results.delta_seconds * 1000.0 / results.delta_ticks) : 0.0;

    this->info_ = info;
    this->time_ms_ = time_ms;
}

const char* ff::internal::debug_timer_model::name_cstr() const
{
    return this->info_.counter ? this->info_.counter->name.c_str() : "";
}

DirectX::XMFLOAT4 ff::internal::debug_timer_model::color() const
{
    switch (this->info_.counter ? this->info_.counter->color : ff::perf_color::white)
    {
        case ff::perf_color::blue:
            return ff::color_blue();

        case ff::perf_color::cyan:
            return ff::color_cyan();

        case ff::perf_color::green:
            return ff::color_green();

        case ff::perf_color::magenta:
            return ff::color_magenta();

        case ff::perf_color::red:
            return ff::color_red();

        case ff::perf_color::yellow:
            return ff::color_yellow();

        default:
            return ff::color_white();
    }
}

double ff::internal::debug_timer_model::time_ms() const
{
    return this->time_ms_;
}

int ff::internal::debug_timer_model::level() const
{
    return static_cast<int>(this->info_.level);
}

int ff::internal::debug_timer_model::hit_total() const
{
    return static_cast<int>(this->info_.hit_total);
}

int ff::internal::debug_timer_model::hit_last_frame() const
{
    return static_cast<int>(this->info_.hit_last_frame);
}

int ff::internal::debug_timer_model::hit_per_second() const
{
    return static_cast<int>(this->info_.hit_per_second);
}

ff::internal::debug_view_model::debug_view_model()
    : pages_(1) // the "none" page
{
    assert(!::global_debug_view_model);
    ::global_debug_view_model = this;
}

ff::internal::debug_view_model::~debug_view_model()
{
    assert(::global_debug_view_model == this);
    ::global_debug_view_model = nullptr;
}

ff::internal::debug_view_model* ff::internal::debug_view_model::get()
{
    assert(::global_debug_view_model);
    return ::global_debug_view_model;
}

bool ff::internal::debug_view_model::dock_right() const
{
    return this->dock_right_;
}

void ff::internal::debug_view_model::dock_right(bool value)
{
    this->dock_right_ = value;
}

double ff::internal::debug_view_model::advance_seconds() const
{
    return this->game_seconds_;
}

void ff::internal::debug_view_model::advance_seconds(double value)
{
    this->game_seconds_ = value;
}

size_t ff::internal::debug_view_model::frames_per_second() const
{
    return this->frames_per_second_;
}

void ff::internal::debug_view_model::frames_per_second(size_t value)
{
    this->frames_per_second_ = value;
}

size_t ff::internal::debug_view_model::frame_count() const
{
    return this->frame_count_;
}

void ff::internal::debug_view_model::frame_count(size_t value)
{
    this->frame_count_ = value;
}

size_t ff::internal::debug_view_model::frame_start_counter() const
{
    return this->frame_start_counter_;
}

void ff::internal::debug_view_model::frame_start_counter(size_t value)
{
    this->frame_start_counter_ = value;
}

bool ff::internal::debug_view_model::debug_visible() const
{
    return this->debug_visible_;
}

void ff::internal::debug_view_model::debug_visible(bool value)
{
    this->debug_visible_ = value;
}

bool ff::internal::debug_view_model::page_picker_visible() const
{
    return this->page_picker_visible_;
}

void ff::internal::debug_view_model::page_picker_visible(bool value)
{
    this->page_picker_visible_ = value;
}

bool ff::internal::debug_view_model::timers_visible() const
{
    return this->timers_visible_;
}

void ff::internal::debug_view_model::timers_visible(bool value)
{
    this->timers_visible_ = value;
}

bool ff::internal::debug_view_model::timers_updating() const
{
    return this->timers_updating_;
}

void ff::internal::debug_view_model::timers_updating(bool value)
{
    this->timers_updating_ = value;
}

size_t ff::internal::debug_view_model::timer_update_speed() const
{
    return this->timer_update_speed_;
}

void ff::internal::debug_view_model::timer_update_speed(size_t value)
{
    value = ff::math::clamp<size_t>(value, 1, 60);
    this->timer_update_speed_ = value;
}

bool ff::internal::debug_view_model::chart_visible() const
{
    return this->chart_visible_;
}

void ff::internal::debug_view_model::chart_visible(bool value)
{
    this->chart_visible_ = value;
}

bool ff::internal::debug_view_model::stopped_visible() const
{
    return this->stopped_visible_;
}

void ff::internal::debug_view_model::stopped_visible(bool value)
{
    this->stopped_visible_ = value;
}

bool ff::internal::debug_view_model::building_resources() const
{
    return ff::global_resources::is_rebuilding();
}

bool ff::internal::debug_view_model::has_pages() const
{
    return !this->pages_.empty();
}

bool ff::internal::debug_view_model::page_visible() const
{
    ff::internal::debug_page_model* page = this->page(this->selected_page());
    return page && !page->is_none() && page->state();
}

size_t ff::internal::debug_view_model::page_count() const
{
    return this->pages_.size();
}

ff::internal::debug_page_model* ff::internal::debug_view_model::page(size_t index) const
{
    return index < this->pages_.size() ? const_cast<ff::internal::debug_page_model*>(&this->pages_[index]) : nullptr;
}

size_t ff::internal::debug_view_model::selected_page() const
{
    return this->selected_page_;
}

void ff::internal::debug_view_model::selected_page(size_t index)
{
    index = index >= this->pages_.size() ? 0 : index;
    this->selected_page_ = index;

    if (this->page(this->selected_page())->is_none())
    {
        this->debug_visible(false);
    }
}

void ff::internal::debug_view_model::add_page(ff::internal::debug_page_model&& page)
{
    this->pages_.emplace_back(std::move(page));
}

void ff::internal::debug_view_model::remove_page(size_t index)
{
    if (index < this->pages_.size())
    {
        this->pages_.erase(this->pages_.begin() + index);
    }
}

size_t ff::internal::debug_view_model::timer_count() const
{
    return this->timers_.size();
}

ff::internal::debug_timer_model* ff::internal::debug_view_model::timer(size_t index) const
{
    return index < this->timers_.size() ? const_cast<ff::internal::debug_timer_model*>(&this->timers_[index]) : nullptr;
}

void ff::internal::debug_view_model::timers(const ff::perf_results& results)
{
    uint32_t i = 0;
    uint32_t timers_count = static_cast<uint32_t>(this->timers_.size());
    for (; i < results.counter_infos.size(); i++)
    {
        if (i < timers_count)
        {
            this->timers_[i].info(results, results.counter_infos[i]);
        }
        else
        {
            this->timers_.emplace_back(results, results.counter_infos[i]);
            timers_count++;
        }
    }

    for (; timers_count > i; --timers_count)
    {
        this->timers_.pop_back();
    }
}

size_t ff::internal::debug_view_model::selected_timer() const
{
    return this->selected_timer_;
}

void ff::internal::debug_view_model::selected_timer(size_t index)
{
    index = index >= this->timers_.size() ? 0 : index;
    this->selected_timer_ = index;
}

void ff::internal::debug_view_model::update_chart(const ff::perf_results& results)
{
    int64_t total_ticks{};
    int64_t render_ticks{};
    int64_t wait_ticks{};

    for (const ff::perf_results::counter_info& info : results.counter_infos)
    {
        switch (info.counter->chart_type)
        {
            case ff::perf_chart_t::frame_total:
                total_ticks += info.ticks;
                break;

            case ff::perf_chart_t::render_total:
                render_ticks += info.ticks;
                break;

            case ff::perf_chart_t::render_wait:
                wait_ticks += info.ticks;
                break;
        }
    }

#if 0
    Noesis::Point* total_points = this->geometry_total_->GetVertices();
    Noesis::Point* render_points = this->geometry_render_->GetVertices();
    Noesis::Point* wait_points = this->geometry_wait_->GetVertices();

    for (size_t i = ::CHART_WIDTH * 2 + 1; i > 1; i--)
    {
        total_points[i].y = total_points[i - 2].y;
        render_points[i].y = render_points[i - 2].y;
        wait_points[i].y = wait_points[i - 2].y;
    }

    float height_scale = results.delta_ticks
        ? static_cast<float>(results.delta_seconds) * ::CHART_HEIGHT_F / (2.0f * ff::constants::seconds_per_advance<float>() * results.delta_ticks)
        : 0.0f;

    wait_points[0].y = ::CHART_HEIGHT_F;
    wait_points[1].y = std::max(0.0f, ::CHART_HEIGHT_F - wait_ticks * height_scale);

    render_points[0].y = wait_points[1].y;
    render_points[1].y = std::max(0.0f, ::CHART_HEIGHT_F - render_ticks * height_scale);

    total_points[0].y = render_points[1].y;
    total_points[1].y = std::max(0.0f, ::CHART_HEIGHT_F - total_ticks * height_scale);

    this->geometry_total_->Update();
    this->geometry_render_->Update();
    this->geometry_wait_->Update();
#endif
}

ff::internal::debug_state::debug_state(const ff::perf_results& perf_results)
    : perf_results(perf_results)
    , input_mapping(ff::internal::app::app_resources().get_resource_object(assets::app::FF_DEBUG_PAGE_INPUT))
    , input_events(std::make_unique<ff::input_event_provider>(*this->input_mapping.object(), std::vector<const ff::input_vk*>{ &ff::input::keyboard() }))
{}

void ff::internal::debug_state::advance_input()
{
    if (this->input_events->advance())
    {
        if (this->input_events->event_hit(::EVENT_CUSTOM))
        {
            ::custom_debug_signal.notify();
        }
        else if (this->input_events->event_hit(::EVENT_TOGGLE_DEBUG))
        {
            this->view_model.debug_visible(!this->view_model.debug_visible());
        }
    }

    ff::state::advance_input();
}

void ff::internal::debug_state::frame_started(ff::state::advance_t type)
{
    ff::internal::debug_view_model* vm = &this->view_model;
    const ff::perf_results& pr = this->perf_results;

    vm->stopped_visible(type == ff::state::advance_t::stopped);

    if (vm->debug_visible())
    {
        vm->advance_seconds(ff::app_time().advance_seconds);
        vm->frame_count(ff::app_time().advance_count);

        if (vm->timers_visible() && vm->timers_updating() && (vm->frame_start_counter() % vm->timer_update_speed()) == 0)
        {
            vm->timers(pr);
        }

        if (vm->chart_visible())
        {
            vm->update_chart(pr);
        }

        for (const ff::perf_results::counter_info& info : pr.counter_infos)
        {
            if (info.counter->chart_type == ff::perf_chart_t::frame_total)
            {
                vm->frames_per_second(info.hit_per_second);
                break;
            }
        }
    }

    ff::state::frame_started(type);

    vm->frame_start_counter(vm->frame_start_counter() + 1); // after UI updates
}

size_t ff::internal::debug_state::child_state_count()
{
    return
        static_cast<size_t>(this->view_model.debug_visible() && this->debug_view_state.get()) +
        static_cast<size_t>(this->view_model.stopped_visible() && this->stopped_view_state.get()) +
        static_cast<size_t>(this->view_model.page_visible() && this->view_model.page(this->view_model.selected_page())->state());
}

ff::state* ff::internal::debug_state::child_state(size_t index)
{
    switch (index)
    {
        case 1:
            if (this->view_model.stopped_visible() && this->view_model.page_visible())
            {
                return this->view_model.page(this->view_model.selected_page())->state();
            }
            break;

        case 0:
            if (this->view_model.stopped_visible())
            {
                return this->stopped_view_state.get();
            }

            if (this->view_model.page_visible())
            {
                return this->view_model.page(this->view_model.selected_page())->state();
            }
            break;
    }

    return this->debug_view_state.get();
}

void ff::add_debug_page(std::string_view name, std::function<std::shared_ptr<ff::state>()>&& factory)
{
    if constexpr (ff::constants::profile_build)
    {
        ff::internal::debug_view_model* vm = ff::internal::debug_view_model::get();
        assert_ret(vm && name.size() && name != ::NONE_NAME);

        for (size_t i = 0; i < vm->page_count(); i++)
        {
            assert_msg_ret(vm->page(i)->name() != name, "Debug page already added");
        }

        vm->add_page(ff::internal::debug_page_model(name, std::move(factory)));
    }
}

void ff::remove_debug_page(std::string_view name)
{
    if constexpr (ff::constants::profile_build)
    {
        ff::internal::debug_view_model* vm = ff::internal::debug_view_model::get();
        assert_ret(vm && name.size() && name != ::NONE_NAME);

        for (size_t i = 0; i < vm->page_count(); i++)
        {
            if (vm->page(i)->name() == name)
            {
                vm->remove_page(i);
                break;
            }
        }
    }
}

void ff::show_debug_page(std::string_view name)
{
    name = name.empty() ? ::NONE_NAME : name;

    if constexpr (ff::constants::profile_build)
    {
        ff::internal::debug_view_model* vm = ff::internal::debug_view_model::get();
        assert_ret(vm);

        for (size_t i = 0; i < vm->page_count(); i++)
        {
            if (vm->page(i)->name() == name)
            {
                vm->selected_page(i);
                break;
            }
        }
    }
}

void ff::debug_visible(bool value)
{
    if constexpr (ff::constants::profile_build)
    {
        ff::internal::debug_view_model* vm = ff::internal::debug_view_model::get();
        assert_ret(vm);
        vm->debug_visible(value);
    }
}

ff::signal_sink<>& ff::custom_debug_sink()
{
    return ::custom_debug_signal;
}
