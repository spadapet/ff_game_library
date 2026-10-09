#include "pch.h"
#include "base/arena.h"
#include "base/assert.h"
#include "base/signal.h"
#include "base/string.h"
#include "data/dict.h"
#include "data/settings.h"
#include "data/stream.h"
#include "data/value.h"

static ff_arena s_settings_arena;
static ff_signal s_settings_save_signal;
static ff_idict s_settings_dict;
static ff_string_view s_settings_file;
static ff_arena_marker s_settings_marker;
static bool s_settings_dirty;

void ff_settings_init(ff_string_view settings_file)
{
    FF_ASSERT(s_settings_marker == NULL);

    ff_arena_init_heap_global(&s_settings_arena, 0);
    ff_signal_init(&s_settings_save_signal);

    s_settings_dict = ff_idict_empty();
    s_settings_file = ff_string_copy(settings_file, &s_settings_arena);
    s_settings_marker = ff_arena_mark(&s_settings_arena);
    s_settings_dirty = false;

    ff_stream stream;
    if (s_settings_file.count && ff_stream_init_read_file(&stream, s_settings_file))
    {
        ff_span dict_span = ff_stream_read(&stream, &s_settings_arena, ff_stream_size(&stream), FF_IDICT_MAX_ALIGN);
        if (!ff_idict_load(&s_settings_dict, dict_span, true, true))
        {
            ff_arena_rewind(&s_settings_arena, s_settings_marker);
            s_settings_dict = ff_idict_empty();
        }

        ff_stream_destroy(&stream);
    }
}

void ff_settings_destroy(void)
{
    FF_ASSERT(s_settings_marker != NULL);

    s_settings_dirty = false;
    s_settings_marker = NULL;
    s_settings_file = ff_string_view_empty();
    s_settings_dict = ff_idict_empty();
    ff_signal_destroy(&s_settings_save_signal);
    ff_arena_destroy(&s_settings_arena);
}

static ff_string_view settings_temp_file(ff_arena* arena)
{
    char* data = ff_arena_alloc_type(arena, char, s_settings_file.count + 5);

    memcpy(data, s_settings_file.data, s_settings_file.count);
    memcpy(data + s_settings_file.count, ".tmp", 5);

    return (ff_string_view)
    {
        .data = data,
        .count = s_settings_file.count + 4,
    };
}

static bool replace_settings_file(ff_string_view temp_file, ff_arena* arena)
{
    ff_wstring_view wide_temp_file = ff_utf8_to_wide(temp_file, arena, true);
    ff_wstring_view wide_settings_file = ff_utf8_to_wide(s_settings_file, arena, true);
    FF_CHECK_RET_VAL(wide_temp_file.count && wide_settings_file.count, false);

    return MoveFileExW(wide_temp_file.data, wide_settings_file.data, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

void ff_settings_save(void)
{
    ff_signal_notify(&s_settings_save_signal, NULL);

    if (s_settings_dirty && s_settings_file.count)
    {
        ff_arena_declare_stack(temp_arena, 1024);
        ff_span dict_span = ff_idict_save(&s_settings_dict, &temp_arena);
        ff_string_view temp_file = settings_temp_file(&temp_arena);
        bool saved = dict_span.size != 0;

        ff_stream stream;
        if (saved && ff_stream_init_write_file(&stream, temp_file))
        {
            saved = ff_stream_write(&stream, dict_span) && FlushFileBuffers(stream.file);
            ff_stream_destroy(&stream);

            if (saved)
            {
                saved = replace_settings_file(temp_file, &temp_arena);
            }
        }
        else
        {
            saved = false;
        }

        if (saved)
        {
            s_settings_dirty = false;
        }

        ff_arena_destroy(&temp_arena);
    }
}

ff_signal* ff_settings_save_signal(void)
{
    return &s_settings_save_signal;
}

ff_idict ff_settings_get(ff_string_view name)
{
    const ff_ivalue* value = ff_idict_get(&s_settings_dict, name);
    return value ? ff_ivalue_as_dict(value, &s_settings_dict) : ff_idict_empty();
}

ff_idict ff_settings_set(ff_string_view name, ff_dict* dict)
{
    s_settings_dirty = true;

    ff_arena temp_arena;
    ff_arena_init_heap_global(&temp_arena, 0);

    ff_dict settings_dict;
    ff_dict_init_from_idict(&settings_dict, &temp_arena, &s_settings_dict);

    if (dict)
    {
        ff_value value_dict = ff_value_new_dict(dict);
        ff_dict_set(&settings_dict, name, &value_dict);
    }
    else
    {
        ff_dict_clear(&settings_dict, name);
    }

    ff_arena_rewind(&s_settings_arena, s_settings_marker);
    ff_idict_init(&s_settings_dict, &s_settings_arena, &settings_dict);
    ff_arena_destroy(&temp_arena);

    return ff_settings_get(name);
}
