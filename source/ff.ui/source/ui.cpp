#include "pch.h"
#include "converters.h"
#include "font_provider.h"
#include "init.h"
#include "key_map.h"
#include "render_device.h"
#include "resource_cache.h"
#include "set_panel_child_focus_action.h"
#include "texture_provider.h"
#include "ui.h"
#include "ui_view.h"
#include "xaml_provider.h"

#define DEBUG_MEM_ALLOC 0

static ff::init_ui_params ui_params;
static std::vector<ff::ui_view*> views;
static std::vector<ff::ui_view*> input_views;
static std::vector<ff::ui_view*> rendered_views;
static Noesis::Ptr<ff::internal::ui::font_provider> font_provider;
static Noesis::Ptr<ff::internal::ui::texture_provider> texture_provider;
static Noesis::Ptr<ff::internal::ui::xaml_provider> xaml_provider;
static Noesis::Ptr<ff::internal::ui::render_device> render_device;
static Noesis::Ptr<Noesis::ResourceDictionary> application_resources;
static ff::ui_view* focused_view;
static Noesis::AssertHandler assert_handler;
static Noesis::ErrorHandler error_handler;
static Noesis::LogHandler log_handler;
static std::vector<ff::input_device_event> device_events;
static ff::signal_connection device_events_connection;
static std::mutex device_events_mutex;

static std::string_view log_levels[] =
{
    "Trace",
    "Debug",
    "Info",
    "Warning",
    "Error",
};

static void noesis_log_handler(const char* filename, uint32_t line, uint32_t level, const char* channel, const char* message)
{
    std::string_view log_level = (level < NS_COUNTOF(log_levels)) ? log_levels[level] : "";
    std::string_view channel2 = channel;
    std::string_view message2 = message;

    ff::log::write(ff::log::type::ui, "[NOESIS/", channel2, "/", log_level, "] ", message2);

    if (::log_handler)
    {
        ::log_handler(filename, line, level, channel, message);
    }
}

static bool noesis_assert_handler(const char* file, uint32_t line, const char* expr)
{
#ifdef _DEBUG
    if (::IsDebuggerPresent())
    {
        __debugbreak();
    }
#endif

    return ::assert_handler ? ::assert_handler(file, line, expr) : false;
}

static void noesis_error_handler(const char* file, uint32_t line, const char* message, bool fatal)
{
    if (::error_handler)
    {
        ::error_handler(file, line, message, fatal);
    }

#ifdef _DEBUG
    if (::IsDebuggerPresent())
    {
        __debugbreak();
    }
#endif
}

static void* noesis_alloc(void* user, size_t size)
{
    void* ptr = std::malloc(size);
#if DEBUG_MEM_ALLOC
    std::ostringstream str;
    str << "[NOESIS/Mem] ALLOC: " << ptr << " (" << size << ")";
    ff::log::write_debug(str);
#endif
    return ptr;
}

static void* noesis_realloc(void* user, void* ptr, size_t size)
{
    void* ptr2 = std::realloc(ptr, size);
#if DEBUG_MEM_ALLOC
    std::ostringstream str;
    str << "[NOESIS/Mem] REALLOC: " << ptr << " -> " << ptr2 << " (" << size << ")";
    ff::log::write_debug(str);
#endif
    return ptr2;
}

static void noesis_dealloc(void* user, void* ptr)
{
#if DEBUG_MEM_ALLOC
    std::ostringstream str;
    str << "[NOESIS/Mem] FREE: " << ptr;
    ff::log::write_debug(str);
#endif
    return std::free(ptr);
}

static size_t noesis_alloc_size(void* user, void* ptr)
{
    size_t size = _msize(ptr);
#if DEBUG_MEM_ALLOC
    std::ostringstream str;
    str << "[NOESIS/Mem] SIZE: " << ptr << " (" << size << ")";
    ff::log::write_debug(str);
#endif
    return size;
}

static void noesis_dump_mem_usage()
{
    ff::log::write(ff::log::type::ui, "NOESIS memory now: ", Noesis::GetAllocatedMemory(), ", Total: ", Noesis::GetAllocatedMemoryAccum());
}

static Noesis::MemoryCallbacks memory_callbacks =
{
    nullptr,
    ::noesis_alloc,
    ::noesis_realloc,
    ::noesis_dealloc,
    ::noesis_alloc_size,
};

static void update_cursor_callback(void* user, Noesis::IView* internal_view, Noesis::Cursor* cursor)
{
    for (ff::ui_view* view : ::views)
    {
        if (view->internal_view() == internal_view)
        {
            view->cursor(cursor ? cursor->Type() : Noesis::CursorType_None);
            break;
        }
    }
}

static void open_url_callback(void* user, const char* url)
{
#if UWP_APP
    Platform::String^ purl = ff::string::to_pstring(std::string_view(url));
    ff::thread_dispatch::get_main()->post([purl]()
        {
            Windows::System::Launcher::LaunchUriAsync(ref new Windows::Foundation::Uri(purl));
        });
#else
    ::ShellExecute(nullptr, L"open", ff::string::to_wstring(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#endif
}

static void play_sound_callback(void* user, const Noesis::Uri& uri, float volume)
{
    // Not implemented
}

static void software_keyboard_callback(void* user, Noesis::UIElement* focused, bool open)
{
    // Not implemented
}

extern "C" void NsInitPackageAppMediaElement();
extern "C" void NsRegisterReflectionAppInteractivity();

static void register_components()
{
    ::NsInitPackageAppMediaElement();
    ::NsRegisterReflectionAppInteractivity();

    Noesis::RegisterComponent<ff::ui::bool_to_collapsed_converter>();
    Noesis::RegisterComponent<ff::ui::bool_to_object_converter>();
    Noesis::RegisterComponent<ff::ui::bool_to_visible_converter>();
    Noesis::RegisterComponent<ff::ui::set_panel_child_focus_action>();

    if (::ui_params.register_components_func)
    {
        ::ui_params.register_components_func();
    }
}

static bool init_noesis()
{
    // Global handlers

    ::assert_handler = Noesis::SetAssertHandler(::noesis_assert_handler);
    ::error_handler = Noesis::SetErrorHandler(::noesis_error_handler);
    ::log_handler = Noesis::SetLogHandler(::noesis_log_handler);
    Noesis::SetMemoryCallbacks(::memory_callbacks);
    Noesis::SetLicense(::ui_params.noesis_license_name.c_str(), ::ui_params.noesis_license_key.c_str());
    Noesis::GUI::DisableInspector();
    Noesis::GUI::Init();

    // Callbacks

    Noesis::GUI::SetCursorCallback(nullptr, ::update_cursor_callback);
    Noesis::GUI::SetOpenUrlCallback(nullptr, ::open_url_callback);
    Noesis::GUI::SetPlayAudioCallback(nullptr, ::play_sound_callback);
    Noesis::GUI::SetSoftwareKeyboardCallback(nullptr, ::software_keyboard_callback);

    // Resource providers

    ::render_device = Noesis::MakePtr<ff::internal::ui::render_device>(::ui_params.srgb);
    ::xaml_provider = Noesis::MakePtr<ff::internal::ui::xaml_provider>();
    ::font_provider = Noesis::MakePtr<ff::internal::ui::font_provider>();
    ::texture_provider = Noesis::MakePtr<ff::internal::ui::texture_provider>();

    Noesis::GUI::SetXamlProvider(::xaml_provider);
    Noesis::GUI::SetTextureProvider(::texture_provider);
    Noesis::GUI::SetFontProvider(::font_provider);

    ::register_components();

    // Default font
    {
        const char* default_fonts = !::ui_params.default_font.empty() ? ::ui_params.default_font.c_str() : "Segoe UI";
        float default_size = ::ui_params.default_font_size > 0.0f ? ::ui_params.default_font_size : 12.0f;
        Noesis::GUI::SetFontFallbacks(&default_fonts, 1);
        Noesis::GUI::SetFontDefaultProperties(default_size, Noesis::FontWeight_Normal, Noesis::FontStretch_Normal, Noesis::FontStyle_Normal);
    }

    // Application resources
    if (!::ui_params.application_resources_name.empty())
    {
        ::application_resources = Noesis::GUI::LoadXaml<Noesis::ResourceDictionary>(::ui_params.application_resources_name.c_str());
        if (::ui_params.application_resources_loaded_func)
        {
            ::ui_params.application_resources_loaded_func(::application_resources);
        }

        Noesis::GUI::SetApplicationResources(::application_resources);
    }

    ::noesis_dump_mem_usage();

    return true;
}

static void destroy_noesis()
{
    for (ff::ui_view* view : std::vector<ff::ui_view*>(::views))
    {
        view->destroy();
    }

    assert(::views.empty());

    ::application_resources.Reset();
    ::xaml_provider.Reset();
    ::font_provider.Reset();
    ::texture_provider.Reset();
    ::render_device.Reset();

    Noesis::GUI::Shutdown();

    ::noesis_dump_mem_usage();
    assert(!Noesis::GetAllocatedMemory());

    Noesis::SetLogHandler(::log_handler);
    ::log_handler = nullptr;

    Noesis::SetErrorHandler(::error_handler);
    ::error_handler = nullptr;

    Noesis::SetAssertHandler(::assert_handler);
    ::assert_handler = nullptr;
}

bool ff::internal::ui::init(const ff::init_ui_params& params)
{
    ::ui_params = params;

    ::device_events_connection = ff::input::combined_devices().event_sink().connect([](const ff::input_device_event& event)
        {
            std::scoped_lock lock(::device_events_mutex);
            ::device_events.push_back(event);
        });

    return true;
}

void ff::internal::ui::destroy()
{
    ::device_events_connection.disconnect();
}

void ff::internal::ui::init_game_thread()
{
    bool status = ::init_noesis();
    assert(status);
}

void ff::internal::ui::destroy_game_thread()
{
    ::destroy_noesis();
}

ff::internal::ui::font_provider* ff::internal::ui::global_font_provider()
{
    return ::font_provider;
}

ff::internal::ui::render_device* ff::internal::ui::global_render_device()
{
    return ::render_device;
}

ff::internal::ui::resource_cache* ff::internal::ui::global_resource_cache()
{
    static ff::internal::ui::resource_cache resource_cache_;
    return &resource_cache_;
}

ff::internal::ui::texture_provider* ff::internal::ui::global_texture_provider()
{
    return ::texture_provider;
}

ff::internal::ui::xaml_provider* ff::internal::ui::global_xaml_provider()
{
    return ::xaml_provider;
}

void ff::internal::ui::register_view(ff::ui_view* view)
{
    if (view && std::find(::views.cbegin(), ::views.cend(), view) == ::views.cend())
    {
        ::views.push_back(view);
    }
}

void ff::internal::ui::unregister_view(ff::ui_view* view)
{
    auto i = std::find(::views.cbegin(), ::views.cend(), view);
    if (i != ::views.cend())
    {
        ::views.erase(i);
    }

    i = std::find(::input_views.cbegin(), ::input_views.cend(), view);
    if (i != ::input_views.cend())
    {
        ::input_views.erase(i);
    }

    i = std::find(::rendered_views.cbegin(), ::rendered_views.cend(), view);
    if (i != ::rendered_views.cend())
    {
        ::rendered_views.erase(i);
    }

    if (::focused_view == view)
    {
        ::focused_view = nullptr;
    }
}

void ff::internal::ui::on_render_view(ff::ui_view* view)
{
    if (std::find(::rendered_views.cbegin(), ::rendered_views.cend(), view) == ::rendered_views.cend())
    {
        ::rendered_views.push_back(view);
    }

    if (view->enabled())
    {
        if (view->block_input_below())
        {
            ::input_views.clear();
        }

        ::input_views.push_back(view);
    }
}

void ff::internal::ui::on_focus_view(ff::ui_view* view, bool focused)
{
    if (focused)
    {
        if (::focused_view && ::focused_view != view)
        {
            ::focused_view->focused(false);
        }

        ::focused_view = view;
    }
}

void ff::ui::state_advance_time()
{
    ff::internal::ui::global_resource_cache()->advance();
}

void ff::ui::state_advance_input()
{
    std::vector<ff::input_device_event> device_events;
    {
        std::scoped_lock lock(::device_events_mutex);
        std::swap(device_events, ::device_events);
    }

    for (const ff::input_device_event& event : device_events)
    {
        switch (event.type)
        {
            case ff::input_device_event_type::key_press:
                if (::focused_view && ff::internal::ui::valid_key(event.id))
                {
                    if (event.count)
                    {
                        ::focused_view->internal_view()->KeyDown(ff::internal::ui::get_key(event.id));
                    }
                    else
                    {
                        ::focused_view->internal_view()->KeyUp(ff::internal::ui::get_key(event.id));
                    }
                }
                break;

            case ff::input_device_event_type::key_char:
                if (::focused_view)
                {
                    ::focused_view->internal_view()->Char(event.id);
                }
                break;

            case ff::input_device_event_type::mouse_press:
                if (ff::internal::ui::valid_mouse_button(event.id))
                {
                    for (auto i = ::input_views.rbegin(); i != ::input_views.rend(); i++)
                    {
                        bool handled = false;
                        ff::ui_view* view = *i;
                        ff::point_float posf = view->screen_to_view(event.pos.cast<float>());
                        ff::point_int pos = posf.cast<int>();

                        if (event.count == 2)
                        {
                            handled = view->internal_view()->MouseDoubleClick(pos.x, pos.y, ff::internal::ui::get_mouse_button(event.id));
                        }
                        else if (event.count == 0)
                        {
                            handled = view->internal_view()->MouseButtonUp(pos.x, pos.y, ff::internal::ui::get_mouse_button(event.id));
                        }
                        else
                        {
                            handled = view->internal_view()->MouseButtonDown(pos.x, pos.y, ff::internal::ui::get_mouse_button(event.id));
                        }

                        if (std::find(::input_views.cbegin(), ::input_views.cend(), view) == ::input_views.cend())
                        {
                            view = nullptr;
                        }

                        if (handled)
                        {
                            if (view)
                            {
                                view->focused(true);
                            }

                            break;
                        }

                        if (view && view->hit_test(posf))
                        {
                            break;
                        }
                    }
                }
                break;

            case ff::input_device_event_type::mouse_move:
                for (auto i = ::input_views.rbegin(); i != ::input_views.rend(); i++)
                {
                    ff::ui_view* view = *i;
                    ff::point_float posf = view->screen_to_view(event.pos.cast<float>());
                    ff::point_int pos = posf.cast<int>();

                    if (view->internal_view()->MouseMove(pos.x, pos.y))
                    {
                        break;
                    }

                    if (std::find(::input_views.cbegin(), ::input_views.cend(), view) == ::input_views.cend())
                    {
                        view = nullptr;
                    }

                    if (view && view->hit_test(posf))
                    {
                        break;
                    }
                }
                break;

            case ff::input_device_event_type::mouse_wheel_x:
                for (auto i = ::input_views.rbegin(); i != ::input_views.rend(); i++)
                {
                    ff::ui_view* view = *i;
                    ff::point_float posf = view->screen_to_view(event.pos.cast<float>());
                    ff::point_int pos = posf.cast<int>();

                    if (view->internal_view()->MouseHWheel(pos.x, pos.y, event.count))
                    {
                        break;
                    }
                }
                break;

            case ff::input_device_event_type::mouse_wheel_y:
                for (auto i = ::input_views.rbegin(); i != ::input_views.rend(); i++)
                {
                    ff::ui_view* view = *i;
                    ff::point_float posf = view->screen_to_view(event.pos.cast<float>());
                    ff::point_int pos = posf.cast<int>();

                    if (view->internal_view()->MouseWheel(pos.x, pos.y, event.count))
                    {
                        break;
                    }
                }
                break;

            case ff::input_device_event_type::touch_press:
                for (auto i = ::input_views.rbegin(); i != ::input_views.rend(); i++)
                {
                    bool handled = false;
                    ff::ui_view* view = *i;
                    ff::point_float posf = view->screen_to_view(event.pos.cast<float>());
                    ff::point_int pos = posf.cast<int>();

                    if (event.count == 0)
                    {
                        handled = view->internal_view()->TouchUp(pos.x, pos.y, event.id);
                    }
                    else
                    {
                        handled = view->internal_view()->TouchDown(pos.x, pos.y, event.id);
                    }

                    if (std::find(::input_views.cbegin(), ::input_views.cend(), view) == ::input_views.cend())
                    {
                        view = nullptr;
                    }

                    if (handled)
                    {
                        if (view)
                        {
                            view->focused(true);
                        }

                        break;
                    }

                    if (view && view->hit_test(posf))
                    {
                        break;
                    }
                }
                break;

            case ff::input_device_event_type::touch_move:
                for (auto i = ::input_views.rbegin(); i != ::input_views.rend(); i++)
                {
                    ff::ui_view* view = *i;
                    ff::point_float posf = view->screen_to_view(event.pos.cast<float>());
                    ff::point_int pos = posf.cast<int>();

                    if (view->internal_view()->TouchMove(pos.x, pos.y, event.id))
                    {
                        break;
                    }

                    if (std::find(::input_views.cbegin(), ::input_views.cend(), view) == ::input_views.cend())
                    {
                        view = nullptr;
                    }

                    if (view && view->hit_test(posf))
                    {
                        break;
                    }
                }
                break;
        }
    }
}

void ff::ui::state_rendering()
{
    // on_render_view will need to be called again for each view actually rendered
    ::input_views.clear();
    ::rendered_views.clear();
}

void ff::ui::state_rendered()
{
    // Fix focus among all views that were actually rendered

    if (::focused_view)
    {
        bool focus = std::find(::input_views.cbegin(), ::input_views.cend(), ::focused_view) != ::input_views.cend() && ff::window::main()->active();
        ::focused_view->focused(focus);
    }

    if ((!::focused_view || !::focused_view->focused()) && !::input_views.empty() && ff::window::main()->active())
    {
        // No visible view has focus, so choose a new one
        ff::ui_view* view = ::input_views.back();
        view->focused(true);
        assert(::focused_view == view);
    }
}

const ff::dxgi::palette_base* ff::ui::global_palette()
{
    return ::ui_params.palette_func ? ::ui_params.palette_func() : nullptr;
}

const std::vector<ff::ui_view*>& ff::ui::input_views()
{
    return ::input_views;
}

const std::vector<ff::ui_view*>& ff::ui::rendered_views()
{
    return ::rendered_views;
}
