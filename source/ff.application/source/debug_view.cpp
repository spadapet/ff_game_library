#include "pch.h"
#include "debug_view.h"
#include "state.h"

using namespace std::string_view_literals;
static std::string_view NONE_NAME = "None"sv;
static Noesis::Ptr<Noesis::ObservableCollection<ff::internal::debug_page_model>> static_pages;

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

ff::state* ff::internal::debug_page_model::state() const
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
    return ff::internal::debug_view_model::static_pages()->Get(0) == this;
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
    NsProp("game_seconds", &ff::internal::debug_view_model::game_seconds, &ff::internal::debug_view_model::game_seconds);
    NsProp("delta_seconds", &ff::internal::debug_view_model::delta_seconds, &ff::internal::debug_view_model::delta_seconds);
    NsProp("frames_per_second", &ff::internal::debug_view_model::frames_per_second, &ff::internal::debug_view_model::frames_per_second);
    NsProp("frame_count", &ff::internal::debug_view_model::frame_count, &ff::internal::debug_view_model::frame_count);
    NsProp("debug_visible", &ff::internal::debug_view_model::debug_visible, &ff::internal::debug_view_model::debug_visible);
    NsProp("extensions_visible", &ff::internal::debug_view_model::extensions_visible, &ff::internal::debug_view_model::extensions_visible);
    NsProp("timers_visible", &ff::internal::debug_view_model::timers_visible, &ff::internal::debug_view_model::timers_visible);
    NsProp("stopped_visible", &ff::internal::debug_view_model::stopped_visible, &ff::internal::debug_view_model::stopped_visible);
    NsProp("has_pages", &ff::internal::debug_view_model::has_pages);
    NsProp("page_visible", &ff::internal::debug_view_model::page_visible);
    NsProp("pages", &ff::internal::debug_view_model::pages);
    NsProp("selected_page", &ff::internal::debug_view_model::selected_page, &ff::internal::debug_view_model::selected_page);
}

ff::internal::debug_view_model::debug_view_model()
    : selected_page_(ff::internal::debug_view_model::static_pages()->Get(0))
{
    this->pages()->CollectionChanged() += Noesis::MakeDelegate(this, &ff::internal::debug_view_model::on_pages_changed);
}

ff::internal::debug_view_model::~debug_view_model()
{
    this->pages()->CollectionChanged() -= Noesis::MakeDelegate(this, &ff::internal::debug_view_model::on_pages_changed);
    ::static_pages.Reset();
}

double ff::internal::debug_view_model::game_seconds() const
{
    return this->game_seconds_;
}

void ff::internal::debug_view_model::game_seconds(double value)
{
    this->set_property(this->game_seconds_, value, "game_seconds");
}

double ff::internal::debug_view_model::delta_seconds() const
{
    return this->delta_seconds_;
}

void ff::internal::debug_view_model::delta_seconds(double value)
{
    this->set_property(this->delta_seconds_, value, "delta_seconds");
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

bool ff::internal::debug_view_model::extensions_visible() const
{
    return this->extensions_visible_;
}

void ff::internal::debug_view_model::extensions_visible(bool value)
{
    this->set_property(this->extensions_visible_, value, "extensions_visible");
}

bool ff::internal::debug_view_model::timers_visible() const
{
    return this->debug_visible_ && this->timers_visible_;
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
    return ff::internal::debug_view_model::static_pages();
}

Noesis::ObservableCollection<ff::internal::debug_page_model>* ff::internal::debug_view_model::static_pages()
{
    if (!::static_pages)
    {
        ::static_pages = Noesis::MakePtr<Noesis::ObservableCollection<ff::internal::debug_page_model>>();
        ::static_pages->Add(Noesis::MakePtr<ff::internal::debug_page_model>());
    }

    return ::static_pages;
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
