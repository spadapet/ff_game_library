#include "pch.h"
#include "base/arena.h"
#include "base/assert.h"
#include "base/log.h"
#include "base/string.h"
#include "base/string_builder.h"

typedef struct log_info
{
    ff_string_view name;
    bool enabled;
} log_info;

static log_info s_log_types[ff_log_type_count] =
{
    { .name = FF_SVL_INIT("ff"), .enabled = false }, // none
    { .name = FF_SVL_INIT("ff/app"), .enabled = true }, // normal
    { .name = FF_SVL_INIT("ff/debug"), .enabled = DEBUG }, // debug
};

static ff_log_sink_data s_log_sink;
static SRWLOCK s_log_mutex = SRWLOCK_INIT;

ff_log_sink_data ff_log_set_sink(ff_log_sink_data sink)
{
    AcquireSRWLockExclusive(&s_log_mutex);
    ff_log_sink_data old_sink = s_log_sink;
    s_log_sink = sink;
    ReleaseSRWLockExclusive(&s_log_mutex);
    return old_sink;
}

ff_string_view ff_log_get_type_name(ff_log_type type)
{
    FF_ASSERT_RET_VAL(type < ff_log_type_count, ff_string_view_empty());

    AcquireSRWLockShared(&s_log_mutex);
    ff_string_view name = s_log_types[type].name;
    ReleaseSRWLockShared(&s_log_mutex);
    return name;
}

bool ff_log_get_type_enabled(ff_log_type type)
{
    FF_ASSERT_RET_VAL(type < ff_log_type_count, false);

    AcquireSRWLockShared(&s_log_mutex);
    bool enabled = s_log_types[type].enabled;
    ReleaseSRWLockShared(&s_log_mutex);
    return enabled;
}

void ff_log_set_type_enabled(ff_log_type type, bool value)
{
    FF_ASSERT_RET(type < ff_log_type_count);

    AcquireSRWLockExclusive(&s_log_mutex);
    s_log_types[type].enabled = value;
    ReleaseSRWLockExclusive(&s_log_mutex);
}

void ff_log_write_v(ff_log_type type, ff_string_view format, va_list args)
{
    FF_ASSERT_RET(type < ff_log_type_count);

    AcquireSRWLockShared(&s_log_mutex);
    bool enabled = s_log_types[type].enabled;
    ff_string_view type_name = s_log_types[type].name;
    ReleaseSRWLockShared(&s_log_mutex);

    if (!enabled)
    {
        return;
    }

    ff_arena_declare_stack(arena, 1024);
    ff_string_builder sb;
    ff_string_builder_init(&sb, &arena);
    ff_string_builder_append(&sb, FF_SVL("["));
    ff_string_builder_append(&sb, type_name);
    ff_string_builder_append(&sb, FF_SVL("] "));
    ff_string_builder_append_format_v(&sb, format, args);
    ff_string_builder_append(&sb, FF_SVL("\r\n"));

    ff_string_view line = ff_string_builder_view(&sb);

    AcquireSRWLockShared(&s_log_mutex);
    if (s_log_sink.sink)
    {
        s_log_sink.sink(type, line, s_log_sink.cookie);
    }
    ReleaseSRWLockShared(&s_log_mutex);

#ifdef _DEBUG
    ff_arena_declare_stack(wide_arena, 1024 * sizeof(wchar_t));
    ff_wstring_view wide_line = ff_utf8_to_wide(line, &wide_arena, true);
    OutputDebugStringW(wide_line.data);
    ff_arena_destroy(&wide_arena);
#endif

    ff_arena_destroy(&arena);
}

void ff_log_write(ff_log_type type, ff_string_view format, ...)
{
    va_list args;
    va_start(args, format);
    ff_log_write_v(type, format, args);
    va_end(args);
}
