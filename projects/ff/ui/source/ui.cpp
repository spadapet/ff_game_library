#include "pch.h"
#include "font_provider.h"
#include "init.h"
#include "render_device.h"
#include "texture_provider.h"
#include "ui.h"
#include "xaml_provider.h"

#define DEBUG_MEM_ALLOC 0

static ff::init_ui_params ui_params;
static std::vector<ff::ui::view*> views;
static std::vector<ff::ui::view*> input_views;
static std::vector<ff::ui::view*> rendered_views;
static Noesis::Ptr<ff::internal::ui::font_provider> font_provider;
static Noesis::Ptr<ff::internal::ui::texture_provider> texture_provider;
static Noesis::Ptr<ff::internal::ui::xaml_provider> xaml_provider;
static Noesis::Ptr<ff::internal::ui::render_device> render_device;
static Noesis::Ptr<Noesis::ResourceDictionary> application_resources;
static ff::ui::view* focused_view;
static Noesis::AssertHandler assert_handler;
static Noesis::ErrorHandler error_handler;
static Noesis::LogHandler log_handler;

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

    std::ostringstream str;
    str << "[NOESIS/" << channel2 << "/" << log_level << "] " << message2;
    ff::log::write_debug(str);

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
    str << "[NOESIS/Mem] REALLOC: " << ptr <<  " -> " << ptr2 << " (" << size << ")";
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

static size_t NoesisAllocSize(void* user, void* ptr)
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
    std::ostringstream str;
    str << "[NOESIS/Mem] Now: " << Noesis::GetAllocatedMemory() << ", Total: " << Noesis::GetAllocatedMemoryAccum();
    ff::log::write_debug(str);
}

static Noesis::MemoryCallbacks memory_callbacks =
{
    nullptr,
    ::noesis_alloc,
    ::noesis_realloc,
    ::noesis_dealloc,
    ::NoesisAllocSize,
};

static bool init_noesis()
{
    return true;
}

static void destroy_noesis()
{
}

bool ff::internal::ui::init(const ff::init_ui_params& params)
{
    ::ui_params = params;

    return ::init_noesis();
}

void ff::internal::ui::destroy()
{
    ::destroy_noesis();
}
