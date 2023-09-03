#include "pch.h"
#include "app.h"
#include "debug_state.h"
#include "ui_view_state.h"

using namespace std::string_view_literals;

static const size_t EVENT_TOGGLE_DEBUG = ff::stable_hash_func("toggle_debug"sv);
static const size_t EVENT_CUSTOM = ff::stable_hash_func("custom_debug"sv);
static std::string_view NONE_NAME = "None"sv;
static ff::internal::debug_view_model* global_debug_view_model{};
static ff::signal<> custom_debug_signal;

NS_IMPLEMENT_REFLECTION(ff::internal::debug_page_model, "ff.debug_page_model")
{
    NsProp("name", &ff::internal::debug_page_model::name_cstr);
    NsProp("is_none", &ff::internal::debug_page_model::is_none);
}

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

NS_IMPLEMENT_REFLECTION(ff::internal::debug_timer_model, "ff.debug_timer_model")
{
    NsProp("name", &ff::internal::debug_timer_model::name_cstr);
    NsProp("name_brush", &ff::internal::debug_timer_model::name_brush);
    NsProp("time_ms", &ff::internal::debug_timer_model::time_ms);
    NsProp("level", &ff::internal::debug_timer_model::level);
    NsProp("hit_total", &ff::internal::debug_timer_model::hit_total);
    NsProp("hit_last_frame", &ff::internal::debug_timer_model::hit_last_frame);
    NsProp("hit_per_second", &ff::internal::debug_timer_model::hit_per_second);
}

ff::internal::debug_timer_model::debug_timer_model(const ff::perf_results& results, const ff::perf_results::counter_info& info)
    : ff::internal::debug_timer_model::debug_timer_model()
{
    this->info(results, info);
}

void ff::internal::debug_timer_model::info(const ff::perf_results& results, const ff::perf_results::counter_info& info)
{
    const double time_ms = results.delta_ticks ? (info.ticks * results.delta_seconds * 1000.0 / results.delta_ticks) : 0.0;

    this->set_property(this->info_.counter, info.counter, "name", "name_brush");
    this->set_property(this->time_ms_, time_ms, "time_ms");
    this->set_property(this->info_.level, info.level, "level");
    this->set_property(this->info_.hit_total, info.hit_total, "hit_total");
    this->set_property(this->info_.hit_last_frame, info.hit_last_frame, "hit_last_frame");
    this->set_property(this->info_.hit_per_second, info.hit_per_second, "hit_per_second");
}

const char* ff::internal::debug_timer_model::name_cstr() const
{
    return this->info_.counter ? this->info_.counter->name.c_str() : "";
}

Noesis::Brush* ff::internal::debug_timer_model::name_brush() const
{
    switch (this->info_.counter ? this->info_.counter->color : ff::perf_color::white)
    {
        case ff::perf_color::blue:
            return Noesis::Brushes::Blue();

        case ff::perf_color::cyan:
            return Noesis::Brushes::Cyan();

        case ff::perf_color::green:
            return Noesis::Brushes::LightGreen();

        case ff::perf_color::magenta:
            return Noesis::Brushes::Magenta();

        case ff::perf_color::red:
            return Noesis::Brushes::Red();

        case ff::perf_color::yellow:
            return Noesis::Brushes::Yellow();

        default:
            return Noesis::Brushes::White();
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

NS_IMPLEMENT_REFLECTION(ff::internal::debug_view_model, "ff.debug_view_model")
{
    NsProp("close_command", &ff::internal::debug_view_model::close_command_);
    NsProp("build_resources_command", &ff::internal::debug_view_model::build_resources_command_);
    NsProp("select_page_command", &ff::internal::debug_view_model::select_page_command_);

    NsProp("advance_seconds", &ff::internal::debug_view_model::advance_seconds, &ff::internal::debug_view_model::advance_seconds);
    NsProp("frames_per_second", &ff::internal::debug_view_model::frames_per_second, &ff::internal::debug_view_model::frames_per_second);
    NsProp("frame_count", &ff::internal::debug_view_model::frame_count, &ff::internal::debug_view_model::frame_count);
    NsProp("frame_start_counter", &ff::internal::debug_view_model::frame_start_counter, &ff::internal::debug_view_model::frame_start_counter);
    NsProp("debug_visible", &ff::internal::debug_view_model::debug_visible, &ff::internal::debug_view_model::debug_visible);
    NsProp("timers_visible", &ff::internal::debug_view_model::timers_visible, &ff::internal::debug_view_model::timers_visible);
    NsProp("timers_updating", &ff::internal::debug_view_model::timers_updating, &ff::internal::debug_view_model::timers_updating);
    NsProp("timers_update_speed", &ff::internal::debug_view_model::timer_update_speed, &ff::internal::debug_view_model::timer_update_speed);
    NsProp("stopped_visible", &ff::internal::debug_view_model::stopped_visible, &ff::internal::debug_view_model::stopped_visible);
    NsProp("building_resources", &ff::internal::debug_view_model::building_resources);
    NsProp("has_pages", &ff::internal::debug_view_model::has_pages);
    NsProp("page_visible", &ff::internal::debug_view_model::page_visible);
    NsProp("pages", &ff::internal::debug_view_model::pages);
    NsProp("timers", &ff::internal::debug_view_model::timers);
    NsProp("selected_page", &ff::internal::debug_view_model::selected_page, &ff::internal::debug_view_model::selected_page);
    NsProp("selected_timer", &ff::internal::debug_view_model::selected_timer, &ff::internal::debug_view_model::selected_timer);
}

ff::internal::debug_view_model::debug_view_model()
    : pages_(Noesis::MakePtr<Noesis::ObservableCollection<ff::internal::debug_page_model>>())
    , timers_(Noesis::MakePtr<Noesis::ObservableCollection<ff::internal::debug_timer_model>>())
    , selected_page_(Noesis::MakePtr<ff::internal::debug_page_model>())
    , close_command_(Noesis::MakePtr<ff::ui::delegate_command>(Noesis::MakeDelegate(this, &ff::internal::debug_view_model::close_command)))
    , build_resources_command_(Noesis::MakePtr<ff::ui::delegate_command>(
        Noesis::MakeDelegate(this, &ff::internal::debug_view_model::build_resources_command),
        Noesis::MakeDelegate(this, &ff::internal::debug_view_model::build_resources_can_execute)))
    , select_page_command_(Noesis::MakePtr<ff::ui::delegate_command>(Noesis::MakeDelegate(this, &ff::internal::debug_view_model::select_page_command)))
    , resource_rebuild_begin_connection(ff::global_resources::rebuild_begin_sink().connect(std::bind(&ff::internal::debug_view_model::on_resources_rebuild_begin, this)))
    , resource_rebuild_end_connection(ff::global_resources::rebuild_end_sink().connect(std::bind(&ff::internal::debug_view_model::on_resources_rebuild_end, this, std::placeholders::_1)))
{
    this->pages_->Add(this->selected_page_);
    this->pages()->CollectionChanged() += Noesis::MakeDelegate(this, &ff::internal::debug_view_model::on_pages_changed);

    assert(!::global_debug_view_model);
    ::global_debug_view_model = this;
}

ff::internal::debug_view_model::~debug_view_model()
{
    this->pages()->CollectionChanged() -= Noesis::MakeDelegate(this, &ff::internal::debug_view_model::on_pages_changed);

    assert(::global_debug_view_model == this);
    ::global_debug_view_model = nullptr;
}

ff::internal::debug_view_model* ff::internal::debug_view_model::get()
{
    assert(::global_debug_view_model);
    return ::global_debug_view_model;
}

double ff::internal::debug_view_model::advance_seconds() const
{
    return this->game_seconds_;
}

void ff::internal::debug_view_model::advance_seconds(double value)
{
    this->set_property(this->game_seconds_, value, "advance_seconds");
}

size_t ff::internal::debug_view_model::frames_per_second() const
{
    return this->frames_per_second_;
}

void ff::internal::debug_view_model::frames_per_second(size_t value)
{
    this->set_property(this->frames_per_second_, value, "frames_per_second");
}

size_t ff::internal::debug_view_model::frame_count() const
{
    return this->frame_count_;
}

void ff::internal::debug_view_model::frame_count(size_t value)
{
    this->set_property(this->frame_count_, value, "frame_count");
}

size_t ff::internal::debug_view_model::frame_start_counter() const
{
    return this->frame_start_counter_;
}

void ff::internal::debug_view_model::frame_start_counter(size_t value)
{
    this->set_property(this->frame_start_counter_, value, "frame_start_counter");
}

bool ff::internal::debug_view_model::debug_visible() const
{
    return this->debug_visible_;
}

void ff::internal::debug_view_model::debug_visible(bool value)
{
    this->set_property(this->debug_visible_, value, "debug_visible");
}

bool ff::internal::debug_view_model::page_picker_visible() const
{
    return this->page_picker_visible_;
}

void ff::internal::debug_view_model::page_picker_visible(bool value)
{
    this->set_property(this->page_picker_visible_, value, "page_picker_visible");
}

bool ff::internal::debug_view_model::timers_visible() const
{
    return this->timers_visible_;
}

void ff::internal::debug_view_model::timers_visible(bool value)
{
    this->set_property(this->timers_visible_, value, "timers_visible");
}

bool ff::internal::debug_view_model::timers_updating() const
{
    return this->timers_updating_;
}

void ff::internal::debug_view_model::timers_updating(bool value)
{
    this->set_property(this->timers_updating_, value, "timers_updating");
}

size_t ff::internal::debug_view_model::timer_update_speed() const
{
    return this->timer_update_speed_;
}

void ff::internal::debug_view_model::timer_update_speed(size_t value)
{
    value = ff::math::clamp<size_t>(value, 1, 60);
    this->set_property(this->timer_update_speed_, value, "timer_update_speed");
}

bool ff::internal::debug_view_model::chart_visible() const
{
    return this->chart_visible_;
}

void ff::internal::debug_view_model::chart_visible(bool value)
{
    this->set_property(this->chart_visible_, value, "chart_visible");
}

bool ff::internal::debug_view_model::stopped_visible() const
{
    return this->stopped_visible_;
}

void ff::internal::debug_view_model::stopped_visible(bool value)
{
    this->set_property(this->stopped_visible_, value, "stopped_visible");
}

bool ff::internal::debug_view_model::building_resources() const
{
    return ff::global_resources::is_rebuilding();
}

bool ff::internal::debug_view_model::has_pages() const
{
    return this->pages()->Count() > 1;
}

bool ff::internal::debug_view_model::page_visible() const
{
    ff::internal::debug_page_model* page = this->selected_page();
    return !page->is_none() && page->state();
}

Noesis::ObservableCollection<ff::internal::debug_page_model>* ff::internal::debug_view_model::pages() const
{
    return this->pages_;
}

ff::internal::debug_page_model* ff::internal::debug_view_model::selected_page() const
{
    return this->selected_page_;
}

void ff::internal::debug_view_model::selected_page(ff::internal::debug_page_model* value)
{
    value = value ? value : this->pages()->Get(0);
    if (this->set_property(this->selected_page_, Noesis::Ptr(value), "selected_page", "page_visible") && !this->selected_page_->is_none())
    {
        this->debug_visible(true);
    }
}

Noesis::ObservableCollection<ff::internal::debug_timer_model>* ff::internal::debug_view_model::timers() const
{
    return this->timers_;
}

void ff::internal::debug_view_model::timers(const ff::perf_results& results) const
{
    uint32_t i = 0;
    uint32_t timers_count = static_cast<uint32_t>(this->timers_->Count());
    for (; i < results.counter_infos.size(); i++)
    {
        if (i < timers_count)
        {
            this->timers_->Get(i)->info(results, results.counter_infos[i]);
        }
        else
        {
            this->timers_->Add(Noesis::MakePtr<ff::internal::debug_timer_model>(results, results.counter_infos[i]));
            timers_count++;
        }
    }

    while (timers_count > i)
    {
        this->timers_->RemoveAt(--timers_count);
    }
}

ff::internal::debug_timer_model* ff::internal::debug_view_model::selected_timer() const
{
    return this->selected_timer_;
}

void ff::internal::debug_view_model::selected_timer(ff::internal::debug_timer_model* value)
{
    this->set_property(this->selected_timer_, Noesis::Ptr(value), "selected_timer");
}

void ff::internal::debug_view_model::on_resources_rebuild_begin()
{
    this->property_changed("building_resources");
    this->build_resources_command_->RaiseCanExecuteChanged();
}

void ff::internal::debug_view_model::on_resources_rebuild_end(size_t round)
{
    if (round == ff::global_resources::rebuild_round_count - 1)
    {
        this->on_resources_rebuild_begin();
    }
}

void ff::internal::debug_view_model::on_pages_changed(Noesis::BaseComponent*, const Noesis::NotifyCollectionChangedEventArgs& args)
{
    if (this->pages()->IndexOf(this->selected_page()) < 0)
    {
        assert(this->selected_page() != this->pages()->Get(0) && this->pages()->Get(0)->is_none());
        this->selected_page(this->pages()->Get(0));
    }

    this->property_changed("has_pages");
}

void ff::internal::debug_view_model::close_command(Noesis::BaseComponent*)
{
    this->debug_visible(false);
}

void ff::internal::debug_view_model::build_resources_command(Noesis::BaseComponent*)
{
    ff::global_resources::rebuild_async();
}

bool ff::internal::debug_view_model::build_resources_can_execute(Noesis::BaseComponent*) const
{
    return !this->building_resources();
}

void ff::internal::debug_view_model::select_page_command(Noesis::BaseComponent* param)
{
    auto page = Noesis::DynamicPtrCast<ff::internal::debug_page_model>(Noesis::Ptr(param));
    if (page)
    {
        this->selected_page(page);
    }
}

NS_IMPLEMENT_REFLECTION(ff::internal::debug_view, "ff.debug_view")
{
    NsProp("view_model", &ff::internal::debug_view::view_model);
}

ff::internal::debug_view::debug_view()
    : view_model_(*new ff::internal::debug_view_model())
{}

ff::internal::debug_view::debug_view(ff::internal::debug_view_model* view_model)
    : view_model_(view_model)
{
    Noesis::GUI::LoadComponent(this, "ff.debug_view.xaml");
}

ff::internal::debug_view_model* ff::internal::debug_view::view_model() const
{
    return this->view_model_;
}

NS_IMPLEMENT_REFLECTION(ff::internal::stopped_view, "ff.stopped_view")
{
    NsProp("view_model", &ff::internal::stopped_view::view_model);
}

ff::internal::stopped_view::stopped_view()
    : view_model_(*new ff::internal::debug_view_model())
{}

ff::internal::stopped_view::stopped_view(ff::internal::debug_view_model* view_model)
    : view_model_(view_model)
{
    Noesis::GUI::LoadComponent(this, "ff.stopped_view.xaml");
}

ff::internal::debug_view_model* ff::internal::stopped_view::view_model() const
{
    return this->view_model_;
}

template<class T>
static std::shared_ptr<ff::ui_view_state> create_view_state(ff::internal::debug_view_model* view_model)
{
    auto noesis_view = Noesis::MakePtr<T>(view_model);
    auto ui_view = std::make_shared<ff::ui_view>(noesis_view);
    return std::make_shared<ff::ui_view_state>(ui_view, ff::ui_view_state::advance_when_t::frame_started);
}

ff::internal::debug_state::debug_state(ff::internal::debug_view_model* view_model, const ff::perf_results& perf_results)
    : perf_results(perf_results)
    , resource_rebuild_end_connection(ff::global_resources::rebuild_end_sink().connect(std::bind(&ff::internal::debug_state::on_resources_rebuild_end, this, std::placeholders::_1)))
    , input_mapping("ff.debug_page_input")
    , input_events(std::make_unique<ff::input_event_provider>(*this->input_mapping.object(), std::vector<const ff::input_vk*>{ &ff::input::keyboard() }))
    , view_model(view_model)
{
    this->init_resources();
}

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
            this->view_model->debug_visible(!this->view_model->debug_visible());
        }
    }

    ff::state::advance_input();
}

void ff::internal::debug_state::frame_started(ff::state::advance_t type)
{
    ff::internal::debug_view_model* vm = this->view_model;
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

        if (pr.counter_infos.size())
        {
            const ff::perf_results::counter_info& info = pr.counter_infos.front();
            vm->frames_per_second(info.hit_per_second);
        }
    }

    ff::state::frame_started(type);

    vm->frame_start_counter(vm->frame_start_counter() + 1);
}

size_t ff::internal::debug_state::child_state_count()
{
    return
        static_cast<size_t>(this->view_model->debug_visible()) +
        static_cast<size_t>(this->view_model->stopped_visible()) +
        static_cast<size_t>(this->view_model->page_visible());
}

ff::state* ff::internal::debug_state::child_state(size_t index)
{
    switch (index)
    {
        case 1:
            if (this->view_model->stopped_visible() && this->view_model->page_visible())
            {
                return this->view_model->selected_page()->state();
            }
            break;

        case 0:
            if (this->view_model->stopped_visible())
            {
                return this->stopped_view_state.get();
            }

            if (this->view_model->page_visible())
            {
                this->view_model->selected_page()->state();
            }
            break;
    }

    return this->debug_view_state.get();
}

void ff::internal::debug_state::init_resources()
{
    this->debug_view_state = ::create_view_state<ff::internal::debug_view>(view_model);
    this->stopped_view_state = ::create_view_state<ff::internal::stopped_view>(view_model);
}

void ff::internal::debug_state::on_resources_rebuild_end(size_t round)
{
    if (round == ff::global_resources::rebuild_round_count)
    {
        this->init_resources();
    }
}

void ff::add_debug_page(std::string_view name, std::function<std::shared_ptr<ff::state>()>&& factory)
{
    if constexpr (ff::constants::profile_build)
    {
        ff::internal::debug_view_model* vm = ff::internal::debug_view_model::get();
        assert_ret(vm && name.size() && name != ::NONE_NAME);

        for (uint32_t i = 0; i < static_cast<uint32_t>(vm->pages()->Count()); i++)
        {
            assert_msg_ret(vm->pages()->Get(i)->name() != name, "Debug page already added");
        }

        vm->pages()->Add(Noesis::MakePtr<ff::internal::debug_page_model>(name, std::move(factory)));
    }
}

void ff::remove_debug_page(std::string_view name)
{
    if constexpr (ff::constants::profile_build)
    {
        ff::internal::debug_view_model* vm = ff::internal::debug_view_model::get();
        assert_ret(vm && name.size() && name != ::NONE_NAME);

        for (uint32_t i = 0; i < static_cast<uint32_t>(vm->pages()->Count()); i++)
        {
            if (vm->pages()->Get(i)->name() == name)
            {
                vm->pages()->RemoveAt(i);
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

        for (uint32_t i = 0; i < static_cast<uint32_t>(vm->pages()->Count()); i++)
        {
            if (vm->pages()->Get(i)->name() == name)
            {
                vm->selected_page(vm->pages()->Get(i));
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
