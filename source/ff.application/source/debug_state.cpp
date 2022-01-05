#include "pch.h"
#include "app.h"
#include "app_time.h"
#include "debug_state.h"
#include "frame_time.h"

using namespace std::string_view_literals;

static const size_t EVENT_TOGGLE_NUMBERS = ff::stable_hash_func("toggle_numbers"sv);
static const size_t EVENT_NEXT_PAGE = ff::stable_hash_func("next_page"sv);
static const size_t EVENT_PREV_PAGE = ff::stable_hash_func("prev_page"sv);
static const size_t EVENT_TOGGLE_0 = ff::stable_hash_func("toggle0"sv);
static const size_t EVENT_TOGGLE_1 = ff::stable_hash_func("toggle1"sv);
static const size_t EVENT_TOGGLE_2 = ff::stable_hash_func("toggle2"sv);
static const size_t EVENT_TOGGLE_3 = ff::stable_hash_func("toggle3"sv);
static const size_t EVENT_TOGGLE_4 = ff::stable_hash_func("toggle4"sv);
static const size_t EVENT_TOGGLE_5 = ff::stable_hash_func("toggle5"sv);
static const size_t EVENT_TOGGLE_6 = ff::stable_hash_func("toggle6"sv);
static const size_t EVENT_TOGGLE_7 = ff::stable_hash_func("toggle7"sv);
static const size_t EVENT_TOGGLE_8 = ff::stable_hash_func("toggle8"sv);
static const size_t EVENT_TOGGLE_9 = ff::stable_hash_func("toggle9"sv);
static const size_t EVENT_CUSTOM = ff::stable_hash_func("custom_debug"sv);

static std::string_view DEBUG_TOGGLE_CHARTS = "Show FPS graph";
static std::string_view DEBUG_PAGE_NAME_0 = "Frame perf";

static std::vector<ff::debug_pages_base*> debug_pages;
static ff::signal<> custom_debug_signal;

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
    , draw_device(ff_dx::draw_device::create())
    , mem_stats{}
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
    , vsync_time(0)
    , render_time(0)
    , flip_time(0)
    , bank_time(0)
    , frames{}
    , frames_end(0)
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
    if (!this->input_events)
    {
        std::vector<const ff::input_vk*> input_devices{ &ff::input::keyboard() };
        this->input_events = std::make_unique<ff::input_event_provider>(*this->input_mapping.object(), std::move(input_devices));
    }

    if (!this->input_events->advance())
    {
        return;
    }

    if (this->input_events->event_hit(::EVENT_CUSTOM) && !this->input_events->event_hit(::EVENT_PREV_PAGE))
    {
        ::custom_debug_signal.notify();
    }
    else if (this->input_events->event_hit(::EVENT_TOGGLE_NUMBERS) && !this->input_events->event_hit(::EVENT_NEXT_PAGE))
    {
        this->enabled_stats = !this->enabled_stats;
    }
    else if (this->enabled_stats)
    {
        if (this->input_events->event_hit(::EVENT_PREV_PAGE))
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
        else if (this->input_events->event_hit(::EVENT_NEXT_PAGE))
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
        else if (this->input_events->event_hit(::EVENT_TOGGLE_0))
        {
            this->toggle(0);
        }
        else if (this->input_events->event_hit(::EVENT_TOGGLE_1))
        {
            this->toggle(1);
        }
        else if (this->input_events->event_hit(::EVENT_TOGGLE_2))
        {
            this->toggle(2);
        }
        else if (this->input_events->event_hit(::EVENT_TOGGLE_3))
        {
            this->toggle(3);
        }
        else if (this->input_events->event_hit(::EVENT_TOGGLE_4))
        {
            this->toggle(4);
        }
        else if (this->input_events->event_hit(::EVENT_TOGGLE_5))
        {
            this->toggle(5);
        }
        else if (this->input_events->event_hit(::EVENT_TOGGLE_6))
        {
            this->toggle(6);
        }
        else if (this->input_events->event_hit(::EVENT_TOGGLE_7))
        {
            this->toggle(7);
        }
        else if (this->input_events->event_hit(::EVENT_TOGGLE_8))
        {
            this->toggle(8);
        }
        else if (this->input_events->event_hit(::EVENT_TOGGLE_9))
        {
            this->toggle(9);
        }
    }
}

void ff::debug_state::render(ff::dxgi::target_base& target, ff::dxgi::depth_base& depth)
{
    this->rps_counter++;
}

void ff::debug_state::frame_rendered(ff::state::advance_t type, ff::dxgi::target_base& target, ff::dxgi::depth_base& depth)
{
    switch (type)
    {
        case ff::state::advance_t::stopped:
            {
                ff::window_size size = target.size();
                ff::point_float rotated_size = size.rotated_pixel_size().cast<float>();
                ff::rect_float target_rect(0, 0, rotated_size.x, rotated_size.y);
                ff::rect_float scaled_target_rect = target_rect / static_cast<float>(target.size().dpi_scale);

                ff::dxgi::draw_ptr draw = this->draw_device->begin_draw(target, nullptr, target_rect, scaled_target_rect);
                if (draw)
                {
                    DirectX::XMFLOAT4 color = ff::dxgi::color_magenta();
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
#ifdef TRACK_MEMORY_ALLOCATIONS
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
            out_color = ff::dxgi::color_magenta();
            str << "Update:" << std::fixed << std::setprecision(2) << this->advance_time_average * 1000.0 << "ms*" << this->advance_count << "/" << std::setprecision(0) << this->last_aps << "Hz";
            break;

        case 1:
            out_color = ff::dxgi::color_green();
            str << "Render:" << std::fixed << std::setprecision(2) << this->render_time * 1000.0 << "ms/" << std::setprecision(0) << this->last_rps << "Hz";
            break;

        case 2:
            str << "Total:" << std::fixed << std::setprecision(2) << (this->advance_time_total + this->vsync_time + this->render_time + this->flip_time) * 1000.0 << "ms\n";
            break;

        case 3:
            out_color = DirectX::XMFLOAT4(.5, .5, .5, 1);
            str << "Memory:" << std::fixed << std::setprecision(3) << this->mem_stats.current / 1000000.0 << " MB (#" << this->mem_stats.count << ")";
            break;
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

    const double freq_d = ff::timer::raw_frequency_double();

    if (update_fast_numbers || !this->advance_count)
    {
        this->advance_count = ft.advance_count;
        this->advance_time_total = advance_time_total_int / freq_d;
        this->advance_time_average = ft.advance_count ? this->advance_time_total / std::min(ft.advance_count, ft.advance_times.size()) : 0.0;
        this->vsync_time = ft.vsync_time / freq_d;
        this->render_time = ft.render_time / freq_d;
        this->flip_time = ft.flip_time / freq_d;
        this->bank_time = gt.unused_advance_seconds;
    }

    ff::debug_state::frame_t frame_info;
    frame_info.advance_time = (float)(advance_time_total_int / freq_d);
    frame_info.render_time = (float)(ft.render_time / freq_d);
    frame_info.total_time = (float)((advance_time_total_int + ft.vsync_time + ft.render_time + ft.flip_time) / freq_d);

    this->frames[this->frames_end] = frame_info;
    this->frames_end = (this->frames_end + 1) % this->frames.size();

    size_t page_index, sub_page_index;
    ff::debug_pages_base* page = convert_page_to_sub_page(this->debug_page, page_index, sub_page_index);
    if (page)
    {
        page->debug_page_update_stats(page_index, update_fast_numbers);
    }
}

void ff::debug_state::render_text(ff::dxgi::target_base& target, ff::dxgi::depth_base& depth)
{
    auto font = this->font.object();
    size_t page_index, sub_page_index;
    ff::debug_pages_base* page = this->convert_page_to_sub_page(this->debug_page, page_index, sub_page_index);
    if (!page)
    {
        return;
    }

    ff::point_float target_size = target.size().rotated_pixel_size().cast<float>();
    ff::rect_float target_rect(ff::point_float(0, 0), target_size);
    ff::dxgi::draw_ptr draw = this->draw_device->begin_draw(target, &depth, target_rect, target_rect / static_cast<float>(target.size().dpi_scale));
    if (draw)
    {
        draw->push_no_overlap();

        std::ostringstream str;
        str << "<F8> Close debug info\n"
            << "<Ctrl-F8> Page " << this->debug_page + 1 << "/" << this->total_page_count() << ": " << page->debug_page_name(sub_page_index) << "\n"
            << "Time:" << std::fixed << std::setprecision(2) << this->total_seconds << "s, FPS:" << std::setprecision(1) << this->last_rps;

        font->draw_text(draw, str.str(), ff::dxgi::transform(ff::point_float(8, 8)), ff::dxgi::color_black());

        size_t line = 3;
        float spacing_y = font->line_spacing();
        float start_y = spacing_y / 2.0f + 8.0f;

        for (size_t i = 0; i < page->debug_page_info_count(sub_page_index); i++, line++)
        {
            DirectX::XMFLOAT4 color = ff::dxgi::color_white();
            std::string str_info;
            page->debug_page_info(sub_page_index, i, str_info, color);

            font->draw_text(draw, str_info, ff::dxgi::transform(ff::point_float(8, start_y + spacing_y * line), ff::point_float(1, 1), 0.0f, color), ff::dxgi::color_black());
        }

        line++;

        for (size_t i = 0; i < page->debug_page_toggle_count(sub_page_index); i++, line++)
        {
            int value = -1;
            std::string str_toggle;
            page->debug_page_toggle_info(sub_page_index, i, str_toggle, value);
            std::string_view str_toggle_value = (value != -1) ? (!value ? " OFF:" : " ON:") : "";

            str.str(std::string());
            str.clear();
            str << "<Ctrl-" << i << ">" << str_toggle_value << " " << str_toggle;

            font->draw_text(draw, str.str(), ff::dxgi::transform(ff::point_float(8, start_y + spacing_y * line)), ff::dxgi::color_black());
        }

        draw->pop_no_overlap();
    }
}

void ff::debug_state::render_charts(ff::dxgi::target_base& target)
{
    const float view_fps = ff::constants::advances_per_second_f;
    const float view_seconds = ff::debug_state::MAX_QUEUE_SIZE / view_fps;
    const float scale = 16;
    const float view_fps_inverse = 1 / view_fps;

    ff::point_float target_size = target.size().rotated_pixel_size().cast<float>();
    ff::rect_float target_rect(ff::point_float(0, 0), target_size);
    ff::rect_float world_rect = ff::rect_float(0, 1, view_seconds, 0);

    ff::dxgi::draw_ptr draw = this->draw_device->begin_draw(target, nullptr, target_rect, world_rect);
    if (draw)
    {
        std::array<ff::point_float, ff::debug_state::MAX_QUEUE_SIZE> advance_points{};
        std::array<ff::point_float, ff::debug_state::MAX_QUEUE_SIZE> render_points{};
        std::array<ff::point_float, ff::debug_state::MAX_QUEUE_SIZE> total_points{};

        ff::point_float points[2] =
        {
            ff::point_float(0, view_fps_inverse * scale),
            ff::point_float(view_seconds, view_fps_inverse * scale),
        };

        draw->draw_line_strip(points, 2, DirectX::XMFLOAT4(1, 1, 0, 1), 1, true);

        for (size_t i = this->frames_end, time_index = 0; ; time_index++)
        {
            float x = static_cast<float>(time_index * view_fps_inverse);
            advance_points[time_index] = ff::point_float(x, this->frames[i].advance_time * scale);
            render_points[time_index] = ff::point_float(x, (this->frames[i].render_time + this->frames[i].advance_time) * scale);
            total_points[time_index] = ff::point_float(x, this->frames[i].total_time * scale);

            i = (i + 1) % this->frames.size();
            if (i == this->frames_end)
            {
                break;
            }
        }

        draw->draw_line_strip(advance_points.data(), advance_points.size(), DirectX::XMFLOAT4(1, 0, 1, 1), 1, true);
        draw->draw_line_strip(render_points.data(), render_points.size(), DirectX::XMFLOAT4(0, 1, 0, 1), 1, true);
        draw->draw_line_strip(total_points.data(), total_points.size(), DirectX::XMFLOAT4(1, 1, 1, 1), 1, true);
    }
}

void ff::debug_state::toggle(size_t index)
{
    size_t page_index, sub_page_index;
    ff::debug_pages_base* page = this->convert_page_to_sub_page(this->debug_page, page_index, sub_page_index);
    if (page)
    {
        if (index < page->debug_page_toggle_count(sub_page_index))
        {
            page->debug_page_toggle(sub_page_index, index);
        }
    }
}

size_t ff::debug_state::total_page_count() const
{
    size_t count = 0;

    for (const ff::debug_pages_base* page : ::debug_pages)
    {
        count += page->debug_page_count();
    }

    return count;
}

ff::debug_pages_base* ff::debug_state::convert_page_to_sub_page(size_t debugPage, size_t& outPage, size_t& outSubPage) const
{
    outPage = 0;
    outSubPage = 0;

    for (ff::debug_pages_base* page : ::debug_pages)
    {
        if (debugPage < page->debug_page_count())
        {
            outSubPage = debugPage;
            return page;
        }

        outPage++;
        debugPage -= page->debug_page_count();
    }

    return nullptr;
}
