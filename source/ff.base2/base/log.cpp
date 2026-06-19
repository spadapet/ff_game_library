#include "pch.h"
#include "base/arena.h"
#include "base/assert.h"
#include "base/log.h"
#include "base/string.h"
#include "base/string_builder.h"

#ifdef _DEBUG
constexpr bool debug_build = true;
#else
constexpr bool debug_build = false;
#endif

struct log_type
{
    ff::string_view name;
    bool enabled;
};

// Indexed by ff::log::type; keep this in sync with the enum order. ff::log::type::count sizes the
// array, so a missing trailing entry would be a zero-initialized (empty-name, disabled) type.
static log_type log_types[(size_t)ff::log::type::count] =
{
    { FF_SVL("ff"), false }, // none
    { FF_SVL("ff/game"), true }, // normal
    { FF_SVL("ff/debug"), ::debug_build }, // debug
};

static ff::log::sink_func log_sink = nullptr;

ff::log::sink_func ff::log::sink(ff::log::sink_func sink)
{
    ff::log::sink_func old_sink = ::log_sink;
    ::log_sink = sink;
    return old_sink;
}

ff::string_view ff::log::type_name(ff::log::type type)
{
    size_t index = (size_t)type;
    FF_ASSERT_RET_VAL(index < (size_t)ff::log::type::count, ff::string_view{});
    return ::log_types[index].name;
}

bool ff::log::type_enabled(ff::log::type type)
{
    size_t index = (size_t)type;
    FF_ASSERT_RET_VAL(index < (size_t)ff::log::type::count, false);
    return ::log_types[index].enabled;
}

void ff::log::type_enabled(ff::log::type type, bool value)
{
    size_t index = (size_t)type;
    FF_ASSERT_RET(index < (size_t)ff::log::type::count);
    ::log_types[index].enabled = value;
}

void ff::log::write_v(ff::log::type type, ff::string_view format, va_list args)
{
    if (!ff::log::type_enabled(type))
    {
        return;
    }

    // Short lines stay on the stack; the arena spills to the process heap only for long messages.
    char buffer[1024];
    ff::arena arena;
    arena.init_external(buffer, sizeof(buffer), 0);

    ff::string_builder sb;
    sb.init(&arena);
    sb.append(FF_SVL("["));
    sb.append(ff::log::type_name(type));
    sb.append(FF_SVL("] "));
    sb.append_format_v(format, args);
    sb.append(FF_SVL("\r\n"));

    ff::string_view line = sb.view();

    if (::log_sink)
    {
        ::log_sink(line);
    }

#ifdef _DEBUG
    wchar_t wide_buffer[1024];
    ff::arena wide_arena;
    wide_arena.init_external(wide_buffer, sizeof(wide_buffer), 0);
    ff::wstring_view wide_line = ff::utf8_to_wide(line, &wide_arena);
    ::OutputDebugStringW(wide_line.data);
    wide_arena.destroy();
#endif

    arena.destroy();
}

void ff::log::write(ff::log::type type, ff::string_view format, ...)
{
    va_list args;
    va_start(args, format);
    ff::log::write_v(type, format, args);
    va_end(args);
}
