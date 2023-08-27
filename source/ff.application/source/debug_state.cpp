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
    NsProp("name", &ff::internal::debug_page_model::name_);
    NsProp("is_none", &ff::internal::debug_page_model::is_none);
    NsProp("removed", &ff::internal::debug_page_model::removed);
}

ff::internal::debug_page_model::debug_page_model()
    : ff::internal::debug_page_model(::NONE_NAME, []() { return std::make_shared<ff::state>(); })
{}

ff::internal::debug_page_model::debug_page_model(std::string_view name, std::function<std::shared_ptr<ff::state>()>&& factory)
    : name_(name.data(), name.data() + name.size())
    , factory(std::move(factory))
{}

std::string_view ff::internal::debug_page_model::name() const
{
    return std::string_view(this->name_.Begin(), this->name_.Size());
}

ff::state* ff::internal::debug_page_model::state()
{
    if (!this->state_ && this->factory)
    {
        auto factory = std::move(this->factory);
        this->state_ = factory();
    }

    return this->state_.get();
}

bool ff::internal::debug_page_model::is_none() const
{
    return this->name() == ::NONE_NAME;
}

bool ff::internal::debug_page_model::removed() const
{
    return this->removed_;
}

void ff::internal::debug_page_model::set_removed()
{
    assert(!this->is_none());
    this->set_property(this->removed_, true, "removed");
}

NS_IMPLEMENT_REFLECTION(ff::internal::debug_view_model, "ff.debug_view_model")
{
    NsProp("close_command", &ff::internal::debug_view_model::close_command_);

    NsProp("advance_seconds", &ff::internal::debug_view_model::advance_seconds, &ff::internal::debug_view_model::advance_seconds);
    NsProp("frames_per_second", &ff::internal::debug_view_model::frames_per_second, &ff::internal::debug_view_model::frames_per_second);
    NsProp("frame_count", &ff::internal::debug_view_model::frame_count, &ff::internal::debug_view_model::frame_count);
    NsProp("debug_visible", &ff::internal::debug_view_model::debug_visible, &ff::internal::debug_view_model::debug_visible);
    NsProp("timers_visible", &ff::internal::debug_view_model::timers_visible, &ff::internal::debug_view_model::timers_visible);
    NsProp("stopped_visible", &ff::internal::debug_view_model::stopped_visible, &ff::internal::debug_view_model::stopped_visible);
    NsProp("has_pages", &ff::internal::debug_view_model::has_pages);
    NsProp("page_visible", &ff::internal::debug_view_model::page_visible);
    NsProp("pages", &ff::internal::debug_view_model::pages);
    NsProp("selected_page", &ff::internal::debug_view_model::selected_page, &ff::internal::debug_view_model::selected_page);
}

ff::internal::debug_view_model::debug_view_model()
    : pages_(Noesis::MakePtr<Noesis::ObservableCollection<ff::internal::debug_page_model>>())
    , selected_page_(Noesis::MakePtr<ff::internal::debug_page_model>())
    , close_command_(Noesis::MakePtr<ff::ui::delegate_command>(Noesis::MakeDelegate(this, &ff::internal::debug_view_model::close_command)))
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

bool ff::internal::debug_view_model::debug_visible() const
{
    return this->debug_visible_;
}

void ff::internal::debug_view_model::debug_visible(bool value)
{
    this->set_property(this->debug_visible_, value, "debug_visible");
}

bool ff::internal::debug_view_model::timers_visible() const
{
    return this->timers_visible_;
}

void ff::internal::debug_view_model::timers_visible(bool value)
{
    this->set_property(this->timers_visible_, value, "timers_visible");
}

bool ff::internal::debug_view_model::stopped_visible() const
{
    return this->stopped_visible_;
}

void ff::internal::debug_view_model::stopped_visible(bool value)
{
    this->set_property(this->stopped_visible_, value, "stopped_visible");
}

bool ff::internal::debug_view_model::has_pages() const
{
    return this->pages()->Count() > 1;
}

bool ff::internal::debug_view_model::page_visible() const
{
    ff::internal::debug_page_model* page = this->selected_page();
    return page && !page->removed() && !page->is_none() && page->state();
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
    if (value && !value->removed())
    {
        this->set_property(this->selected_page_, Noesis::Ptr(value), "selected_page", "page_visible");
    }
}

void ff::internal::debug_view_model::on_pages_changed(Noesis::BaseComponent*, const Noesis::NotifyCollectionChangedEventArgs& args)
{
    if (this->selected_page()->removed())
    {
        assert(this->selected_page() != this->pages()->Get(0));
        this->selected_page(this->pages()->Get(0));
    }

    this->property_changed("has_pages");
}

void ff::internal::debug_view_model::close_command(Noesis::BaseComponent*)
{
    this->debug_visible(false);
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
    , input_mapping("ff.debug_page_input")
    , input_events(std::make_unique<ff::input_event_provider>(*this->input_mapping.object(), std::vector<const ff::input_vk*>{ &ff::input::keyboard() }))
    , view_model(view_model)
    , debug_view_state(::create_view_state<ff::internal::debug_view>(view_model))
    , stopped_view_state(::create_view_state<ff::internal::stopped_view>(view_model))
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
            this->view_model->debug_visible(!this->view_model->debug_visible());
        }
    }

    ff::state::advance_input();
}

void ff::internal::debug_state::frame_started(ff::state::advance_t type)
{
    this->view_model->stopped_visible(type == ff::state::advance_t::stopped);

    if (this->view_model->debug_visible())
    {
        this->view_model->advance_seconds(ff::app_time().advance_seconds);
        this->view_model->frame_count(ff::app_time().advance_count);

        if (this->perf_results.counter_infos.size())
        {
            const ff::perf_results::counter_info& info = this->perf_results.counter_infos.front();
            this->view_model->frames_per_second(info.hit_per_second);
        }
    }

    ff::state::frame_started(type);
}

size_t ff::internal::debug_state::child_state_count()
{
    return
        (this->view_model->debug_visible() ? 1 : 0) +
        (this->view_model->stopped_visible() ? 1 : 0) +
        (this->view_model->page_visible() ? 1 : 0);
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

void ff::add_debug_page(std::string_view name, std::function<std::shared_ptr<ff::state>()>&& factory)
{
    if constexpr (ff::constants::profile_build)
    {
        if (ff::internal::debug_view_model::get())
        {
            Noesis::ObservableCollection<ff::internal::debug_page_model>* pages = ff::internal::debug_view_model::get()->pages();
            for (uint32_t i = 0; i < static_cast<uint32_t>(pages->Count()); i++)
            {
                ff::internal::debug_page_model* page = pages->Get(i);
                if (page->name() == name)
                {
                    // already added
                    return;
                }
            }

            auto page = Noesis::MakePtr<ff::internal::debug_page_model>(name, std::move(factory));
            pages->Add(page);
        }
    }
}

void ff::remove_debug_page(std::string_view name)
{
    if constexpr (ff::constants::profile_build)
    {
        if (ff::internal::debug_view_model::get())
        {
            Noesis::ObservableCollection<ff::internal::debug_page_model>* pages = ff::internal::debug_view_model::get()->pages();
            for (uint32_t i = 0; i < static_cast<uint32_t>(pages->Count()); i++)
            {
                ff::internal::debug_page_model* page = pages->Get(i);
                if (!page->is_none() && name == page->name())
                {
                    page->set_removed();
                    pages->RemoveAt(i);
                    break;
                }
            }
        }
    }
}

void ff::show_debug_page(std::string_view name)
{
    if constexpr (ff::constants::profile_build)
    {
        if (ff::internal::debug_view_model::get())
        {
            ff::internal::debug_view_model* vm = ff::internal::debug_view_model::get();
            Noesis::ObservableCollection<ff::internal::debug_page_model>* pages = vm->pages();

            for (uint32_t i = 0; i < static_cast<uint32_t>(pages->Count()); i++)
            {
                ff::internal::debug_page_model* page = pages->Get(i);
                if (page->name() == name || (!name.size() && page->is_none()))
                {
                    vm->selected_page(page);
                    vm->debug_visible(true);
                    break;
                }
            }
        }
    }
}

void ff::debug_visible(bool value)
{
    if constexpr (ff::constants::profile_build)
    {
        if (ff::internal::debug_view_model::get())
        {
            ff::internal::debug_view_model::get()->debug_visible(value);
        }
    }
}

ff::signal_sink<>& ff::custom_debug_sink()
{
    return ::custom_debug_signal;
}
