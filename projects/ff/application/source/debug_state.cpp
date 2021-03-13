#include "pch.h"
#include "app.h"
#include "app_time.h"
#include "debug_state.h"
#include "frame_time.h"

namespace ff
{
    class debug_state : public ff::state, public ff::debug_pages_base
    {
    public:
        debug_state();
        debug_state(debug_state&& other) noexcept = delete;
        debug_state(const debug_state& other) = delete;
        virtual ~debug_state() override;

        debug_state& operator=(debug_state&& other) noexcept = delete;
        debug_state& operator=(const debug_state& other) = delete;

        // state
        virtual std::shared_ptr<ff::state> advance_time() override;
        virtual void advance_input() override;
        virtual void render(ff::dx11_target_base& target, ff::dx11_depth& depth) override;
        virtual void frame_rendered(ff::state::advance_t type, ff::dx11_target_base& target, ff::dx11_depth& depth) override;
        virtual ff::state::status_t status() override;

        // debug_pages_base
        virtual size_t debug_page_count() const override;
        virtual std::string_view debug_page_name(size_t page) const override;
        virtual void debug_page_update_stats(size_t page, bool update_fast_numbers) override;
        virtual size_t debug_page_info_count(size_t page) const override;
        virtual void debug_page_info(size_t page, size_t index, std::string& out_text, DirectX::XMFLOAT4& out_color) const override;
        virtual size_t debug_page_toggle_count(size_t page) const override;
        virtual void debug_page_toggle_info(size_t page, size_t index, std::string& out_text, int& out_value) const override;
        virtual void debug_page_toggle(size_t page, size_t index) override;

    private:
        void update_stats();
        void render_text(ff::dx11_target_base& target, ff::dx11_depth& depth);
        void render_charts(ff::dx11_target_base& target);
        void toggle(size_t index);
        size_t total_page_count() const;
        ff::debug_pages_base* convert_page_to_sub_page(size_t debugPage, size_t& outPage, size_t& outSubPage) const;

        static constexpr size_t MAX_QUEUE_SIZE = ff::constants::advances_per_second_s * 6; // six seconds of data

        bool enabled_stats;
        bool enabled_charts;
        size_t debug_page;
        ff::auto_resource<ff::sprite_font> font;
        ff::auto_resource<ff::input_mapping> input_mapping;
        ff::input_event_provider input_events;
        std::unique_ptr<ff::draw_device> draw_device;

        ff::memory::allocation_stats mem_stats;
        ff::graphics_counters graphics_counters;
        size_t total_advance_count;
        size_t total_render_count;
        size_t advance_count;
        size_t fast_number_counter;
        size_t aps_counter;
        size_t rps_counter;
        double last_aps;
        double last_rps;
        double total_seconds;
        double old_seconds;
        double advance_time_total;
        double advance_time_average;
        double render_time;
        double flip_time;
        double bank_time;

        struct frame_t
        {
            float advance_time;
            float render_time;
            float total_time;
        };

        std::array<ff::debug_state::frame_t, MAX_QUEUE_SIZE> frames;
        size_t frames_start;
        size_t frames_end;
    };

    void add_debug_pages(ff::debug_pages_base* pages);
    void remove_debug_pages(ff::debug_pages_base* pages);
    ff::signal_sink<>& custom_debug_sink();
}

static const size_t EVENT_TOGGLE_NUMBERS = ff::stable_hash_func("toggle_numbers");
static const size_t EVENT_NEXT_PAGE = ff::stable_hash_func(L"next_page");
static const size_t EVENT_PREV_PAGE = ff::stable_hash_func(L"prev_page");
static const size_t EVENT_TOGGLE_0 = ff::stable_hash_func(L"toggle0");
static const size_t EVENT_TOGGLE_1 = ff::stable_hash_func(L"toggle1");
static const size_t EVENT_TOGGLE_2 = ff::stable_hash_func(L"toggle2");
static const size_t EVENT_TOGGLE_3 = ff::stable_hash_func(L"toggle3");
static const size_t EVENT_TOGGLE_4 = ff::stable_hash_func(L"toggle4");
static const size_t EVENT_TOGGLE_5 = ff::stable_hash_func(L"toggle5");
static const size_t EVENT_TOGGLE_6 = ff::stable_hash_func(L"toggle6");
static const size_t EVENT_TOGGLE_7 = ff::stable_hash_func(L"toggle7");
static const size_t EVENT_TOGGLE_8 = ff::stable_hash_func(L"toggle8");
static const size_t EVENT_TOGGLE_9 = ff::stable_hash_func(L"toggle9");
static const size_t EVENT_CUSTOM = ff::stable_hash_func(L"custom_debug");

static std::string_view DEBUG_TOGGLE_CHARTS("Show FPS graph");
static std::string_view DEBUG_PAGE_NAME_0("Frame perf");

static std::vector<ff::debug_pages_base*> debug_pages;
static ff::signal<void> custom_debug_signal;

void ff::add_debug_pages(ff::debug_pages_base* pages)
{
    if (pages && std::find(::debug_pages.cbegin(), ::debug_pages.cend(), pages) == ::debug_pages.cend())
    {
        ::debug_pages.push_back(pages);
    }
}

void ff::remove_debug_pages(ff::debug_pages_base* pages)
{
    auto i = std::find(::debug_pages.cbegin(), ::debug_pages.cend(), pages);
    if (i != ::debug_pages.cend())
    {
        ::debug_pages.erase(i);
    }
}

ff::signal_sink<>& ff::custom_debug_sink()
{
    return ::custom_debug_signal;
}

ff::debug_state::debug_state()
    : enabled_stats(false)
    , enabled_charts(false)
    , debug_page(0)
    , font("ff.debug_font")
    , input_mapping("ff.debug_page_input")
    , input_events(*this->input_mapping.object(), std::vector<ff::input_vk*>{ &ff::input::keyboard() })
    , draw_device(ff::draw_device::create())
    , mem_stats{}
    , graphics_counters{}
    , total_advance_count(0)
    , total_render_count(0)
    , advance_count(0)
    , fast_number_counter(0)
    , aps_counter(0)
    , rps_counter(0)
    , last_aps(0)
    , last_rps(0)
    , total_seconds(0)
    , old_seconds(0)
    , advance_time_total(0)
    , advance_time_average(0)
    , render_time(0)
    , flip_time(0)
    , bank_time(0)
{
    ff::add_debug_pages(this);
}

ff::debug_state::~debug_state()
{
    ff::remove_debug_pages(this);
}

std::shared_ptr<ff::state> ff::debug_state::advance_time()
{
    this->aps_counter++;

    return nullptr;
}

void ff::debug_state::advance_input()
{
    if (!this->input_events.advance(ff::constants::advances_per_second))
    {
        return;
    }

    if (this->input_events.event_hit(::EVENT_CUSTOM) && !this->input_events.event_hit(::EVENT_PREV_PAGE))
    {
        ::custom_debug_signal.notify();
    }
    else if (this->input_events.event_hit(::EVENT_TOGGLE_NUMBERS) && !this->input_events.event_hit(::EVENT_NEXT_PAGE))
    {
        this->enabled_stats = !this->enabled_stats;
    }
    else if (this->enabled_stats)
    {
        if (this->input_events.event_hit(::EVENT_PREV_PAGE))
        {
            if (this->debug_page == 0 && this->total_page_count() > 0)
            {
                this->debug_page = this->total_page_count() - 1;
            }
            else if (this->debug_page > 0)
            {
                this->debug_page--;
            }
        }
        else if (this->input_events.event_hit(::EVENT_NEXT_PAGE))
        {
            if (this->debug_page + 1 >= this->total_page_count())
            {
                this->debug_page = 0;
            }
            else
            {
                this->debug_page++;
            }
        }
        else if (this->input_events.event_hit(::EVENT_TOGGLE_0))
        {
            this->toggle(0);
        }
        else if (this->input_events.event_hit(::EVENT_TOGGLE_1))
        {
            this->toggle(1);
        }
        else if (this->input_events.event_hit(::EVENT_TOGGLE_2))
        {
            this->toggle(2);
        }
        else if (this->input_events.event_hit(::EVENT_TOGGLE_3))
        {
            this->toggle(3);
        }
        else if (this->input_events.event_hit(::EVENT_TOGGLE_4))
        {
            this->toggle(4);
        }
        else if (this->input_events.event_hit(::EVENT_TOGGLE_5))
        {
            this->toggle(5);
        }
        else if (this->input_events.event_hit(::EVENT_TOGGLE_6))
        {
            this->toggle(6);
        }
        else if (this->input_events.event_hit(::EVENT_TOGGLE_7))
        {
            this->toggle(7);
        }
        else if (this->input_events.event_hit(::EVENT_TOGGLE_8))
        {
            this->toggle(8);
        }
        else if (this->input_events.event_hit(::EVENT_TOGGLE_9))
        {
            this->toggle(9);
        }
    }
}

void ff::debug_state::render(ff::dx11_target_base& target, ff::dx11_depth& depth)
{
    this->rps_counter++;
}

void ff::debug_state::frame_rendered(ff::state::advance_t type, ff::dx11_target_base& target, ff::dx11_depth& depth)
{
    switch (type)
    {
        case ff::state::advance_t::stopped:
            {
                ff::point_float target_size = target.size().rotated_pixel_size().cast<float>();
                ff::rect_float target_rect = target_size;
                ff::rect_float scaled_target_rect = target_size / static_cast<float>(target.size().dpi_scale);

                ff::draw_ptr draw = this->draw_device->begin_draw(target, nullptr, target_rect, scaled_target_rect);
                if (draw)
                {
                    DirectX::XMFLOAT4 color = ff::color::magenta();
                    color.w = 0.375;
                    draw->draw_outline_rectangle(scaled_target_rect, color, std::min<size_t>(this->fast_number_counter++, 16) / 2.0f, true);
                }
            }
            break;

        case ff::state::advance_t::single_step:
            this->fast_number_counter = 0;
            break;
    }

    if (this->enabled_stats)
    {
        if (type != ff::state::advance_t::stopped)
        {
            this->update_stats();
        }

        this->render_text(target, depth);

        if (this->enabled_charts)
        {
            this->render_charts(target);
        }
    }
}

ff::state::status_t ff::debug_state::status()
{
    return ff::state::status_t::ignore;
}

size_t ff::debug_state::debug_page_count() const
{
    return 1;
}

std::string_view ff::debug_state::debug_page_name(size_t page) const
{
    return ::DEBUG_PAGE_NAME_0;
}

void ff::debug_state::debug_page_update_stats(size_t page, bool update_fast_numbers)
{}

size_t ff::debug_state::debug_page_info_count(size_t page) const
{
#ifdef _DEBUG
    return 4;
#else
    return 3;
#endif
}

void ff::debug_state::debug_page_info(size_t page, size_t index, std::string& out_text, DirectX::XMFLOAT4& out_color) const
{
    std::ostringstream str;

    switch (index)
    {
        case 0:
            out_color = ff::color::magenta();
            str << "Update:%.2fms*%Iu/%.fHz"; // this->advance_time_average * 1000.0, this->advance_count, this->last_aps
            break;

        case 1:
            out_color = ff::color::green();
            str << "Render:%.2fms/%.fHz, Clear:T%lu/D%lu, Draw:%lu"; // this->render_time * 1000.0, this->last_rps, this->graphics_counters._clear, this->graphics_counters._depthClear, this->graphics_counters._draw
            break;

        case 2:
            str << L"Total:%.2fms\n"; // (this->advance_time_total + this->render_time + this->flip_time) * 1000.0;

        case 3:
            out_color = DirectX::XMFLOAT4(.5, .5, .5, 1);
            str << "Memory:%.3f MB (#%lu)"; // this->mem_stats.current / 1000000.0, this->mem_stats.count;
    }

    out_text = str.str();
}

size_t ff::debug_state::debug_page_toggle_count(size_t page) const
{
    return 1;
}

void ff::debug_state::debug_page_toggle_info(size_t page, size_t index, std::string& out_text, int& out_value) const
{
    switch (index)
    {
        case 0:
            out_value = this->enabled_charts;
            out_text = ::DEBUG_TOGGLE_CHARTS;
            break;

        default:
            out_value = 0;
            out_text.clear();
            break;
    }
}

void ff::debug_state::debug_page_toggle(size_t page, size_t index)
{
    switch (index)
    {
        case 0:
            this->enabled_charts = !this->enabled_charts;
            break;
    }
}

void ff::debug_state::update_stats()
{
    const ff::frame_time_t& ft = ff::frame_time();
    const ff::app_time_t& gt = ff::app_time();
    bool update_fast_numbers = !(this->fast_number_counter++ % 8);

    INT64 advance_time_total_int = 0;
    for (size_t i = 0; i < ft.advance_count && i < ft.advance_times.size(); i++)
    {
        advance_time_total_int += ft.advance_times[i];
    }

    this->mem_stats = ff::memory::get_allocation_stats();
    this->total_advance_count = gt.advance_count;
    this->total_render_count = gt.render_count;
    this->total_seconds = gt.clock_seconds;

    if (std::floor(this->old_seconds) != std::floor(this->total_seconds))
    {
        this->last_aps = this->aps_counter / (this->total_seconds - this->old_seconds);
        this->last_rps = this->rps_counter / (this->total_seconds - this->old_seconds);
        this->old_seconds = this->total_seconds;
        this->aps_counter = 0;
        this->rps_counter = 0;
    }

    const double freq_d = ff::timer::raw_frequency_double_static();

    if (update_fast_numbers || !this->advance_count)
    {
        this->advance_count = ft.advance_count;
        this->advance_time_total = advance_time_total_int / freq_d;
        this->advance_time_average = ft.advance_count ? this->advance_time_total / std::min(ft.advance_count, ft.advance_times.size()) : 0.0;
        this->render_time = ft.render_time / freq_d;
        this->flip_time = ft.flip_time / freq_d;
        this->bank_time = gt.unused_advance_seconds;
        this->graphics_counters = ft.graphics_counters;
    }

    ff::debug_state::frame_t frame_info;
    frame_info.advance_time = (float)(advance_time_total_int / freq_d);
    frame_info.render_time = (float)(ft.render_time / freq_d);
    frame_info.total_time = (float)((advance_time_total_int + ft.render_time + ft.flip_time) / freq_d);

    // TODO: Circular buffer
    this->frames[0] = frame_info;

    size_t pageIndex, subPageIndex;
    ff::debug_pages_base* page = convert_page_to_sub_page(this->debug_page, pageIndex, subPageIndex);
    if (page)
    {
        page->DebugUpdateStats(globals, pageIndex, update_fast_numbers);
    }
}

void ff::debug_state::render_text(ff::dx11_target_base& target, ff::dx11_depth& depth)
{
    ff::ISpriteFont* font = this->font.GetObject();
    noAssertRet(font);

    size_t pageIndex, subPageIndex;
    ff::ff::debug_pages_base* page = convert_page_to_sub_page(this->debug_page, pageIndex, subPageIndex);
    noAssertRet(page);

    ff::point_float target_size = target->GetRotatedSize().ToType<float>();
    ff::rect_float target_rect(target_size);

    ff::RendererActive render = _render->BeginRender(target, depth, target_rect, target_rect / (float)target->GetDpiScale());
    if (render)
    {
        render->PushNoOverlap();

        ff::String introText = ff::String::format_new(
            L"<F8> Close debug info\n"
            L"<Ctrl-F8> Page %lu/%lu: %s\n"
            L"Time:%.2fs, FPS:%.1f",
            this->debug_page + 1,
            this->total_page_count(),
            page ? page->GetDebugName(subPageIndex).c_str() : L"",
            this->total_seconds,
            this->last_rps);

        font->DrawText(render, introText, ff::Transform::Create(ff::point_float(8, 8)), ff::GetColorBlack());

        size_t line = 3;
        float spacingY = font->GetLineSpacing();
        float startY = spacingY / 2.0f + 8.0f;

        for (size_t i = 0; i < page->GetDebugInfoCount(subPageIndex); i++, line++)
        {
            DirectX::XMFLOAT4 color = ff::GetColorWhite();
            ff::String str = page->GetDebugInfo(subPageIndex, i, color);

            font->DrawText(render, str, ff::Transform::Create(ff::point_float(8, startY + spacingY * line), ff::point_float::Ones(), 0.0f, color), ff::GetColorBlack());
        }

        line++;

        for (size_t i = 0; i < page->GetDebugToggleCount(subPageIndex); i++, line++)
        {
            int value = -1;
            ff::String str = page->GetDebugToggle(subPageIndex, i, value);
            const wchar_t* toggleText = (value != -1) ? (!value ? L" OFF:" : L" ON:") : L"";

            font->DrawText(render, ff::String::format_new(L"<Ctrl-%lu>%s %s", i, toggleText, str.c_str()), ff::Transform::Create(ff::point_float(8, startY + spacingY * line)), ff::GetColorBlack());
        }

        render->PopNoOverlap();
    }
}

void ff::debug_state::render_charts(ff::dx11_target_base& target)
{
    const float viewFps = ff::ADVANCES_PER_SECOND_F;
    const float viewSeconds = MAX_QUEUE_SIZE / viewFps;
    const float scale = 16;
    const float viewFpsInverse = 1 / viewFps;

    ff::point_float target_size = target->GetRotatedSize().ToType<float>();
    ff::rect_float target_rect(target_size);
    ff::rect_float worldRect = ff::rect_float(-viewSeconds, 1, 0, 0);

    ff::RendererActive render = _render->BeginRender(target, nullptr, target_rect, worldRect);
    if (render)
    {
        ff::point_float advancePoints[MAX_QUEUE_SIZE];
        ff::point_float renderPoints[MAX_QUEUE_SIZE];
        ff::point_float totalPoints[MAX_QUEUE_SIZE];

        ff::point_float points[2] =
        {
            ff::point_float(0, viewFpsInverse * scale),
            ff::point_float(-viewSeconds, viewFpsInverse * scale),
        };

        render->DrawLineStrip(points, 2, DirectX::XMFLOAT4(1, 1, 0, 1), 1, true);

        if (this->frames.Size() > 1)
        {
            size_t timeIndex = 0;
            for (auto i = this->frames.rbegin(); i != this->frames.rend(); i++, timeIndex++)
            {
                float x = (float)(timeIndex * -viewFpsInverse);
                advancePoints[timeIndex].SetPoint(x, i->advance_time * scale);
                renderPoints[timeIndex].SetPoint(x, (i->render_time + i->advance_time) * scale);
                totalPoints[timeIndex].SetPoint(x, i->total_time * scale);
            }

            render->DrawLineStrip(advancePoints, this->frames.Size(), DirectX::XMFLOAT4(1, 0, 1, 1), 1, true);
            render->DrawLineStrip(renderPoints, this->frames.Size(), DirectX::XMFLOAT4(0, 1, 0, 1), 1, true);
            render->DrawLineStrip(totalPoints, this->frames.Size(), DirectX::XMFLOAT4(1, 1, 1, 1), 1, true);
        }
    }
}

void ff::debug_state::toggle(size_t index)
{
    size_t pageIndex, subPageIndex;
    ff::debug_pages_base* page = convert_page_to_sub_page(this->debug_page, pageIndex, subPageIndex);
    noAssertRet(page);

    if (index < page->GetDebugToggleCount(subPageIndex))
    {
        page->DebugToggle(subPageIndex, index);
    }
}

size_t ff::debug_state::total_page_count() const
{
    size_t count = 0;

    for (const ff::debug_pages_base* page : _globals->GetDebugPages())
    {
        count += page->GetDebugPageCount();
    }

    return count;
}

ff::ff::debug_pages_base* ff::debug_state::convert_page_to_sub_page(size_t debugPage, size_t& outPage, size_t& outSubPage) const
{
    outPage = 0;
    outSubPage = 0;

    for (ff::debug_pages_base* page : _globals->GetDebugPages())
    {
        if (debugPage < page->GetDebugPageCount())
        {
            outSubPage = debugPage;
            return page;
        }

        outPage++;
        debugPage -= page->GetDebugPageCount();
    }

    return nullptr;
}
