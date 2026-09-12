#pragma once

#include "../base/span.h"
#include "../base/string.h"

typedef struct ff_arena ff_arena;

typedef enum ff_stream_type
{
    ff_stream_type_none,
    ff_stream_type_read_file,
    ff_stream_type_write_file,
    ff_stream_type_read_memory,
    ff_stream_type_write_memory,
} ff_stream_type;

// Callers own this struct (usually on the stack) and hand it to one of the init functions below.
// The fields are visible so the type stays POD and debuggable, but treat them as read-only.
typedef struct ff_stream
{
    ff_stream_type type;

    // File streams
    HANDLE file;

    // Memory streams
    ff_arena* arena;
    uint8_t* data;
    size_t capacity;

    // Read streams: 'size' is the total and 'pos' is the read cursor.
    // Write streams: 'size' is the bytes written and 'pos' is unused.
    size_t size;
    size_t pos;
} ff_stream;

// Opens an existing file for reading. Returns false (and leaves an unusable stream) on failure.
bool ff_stream_init_read_file(ff_stream* stream, ff_string_view path);

// Creates (or truncates) a file for writing. Returns false on failure.
bool ff_stream_init_write_file(ff_stream* stream, ff_string_view path);

// Reads from memory owned by the caller, which must stay valid until the stream is destroyed.
void ff_stream_init_read_memory(ff_stream* stream, ff_span span);

// Writes to a growable buffer allocated from 'arena'. Since the buffer belongs to the arena, it
// stays valid after the stream is destroyed (until the arena is reset, rewound, or destroyed).
bool ff_stream_init_write_memory(ff_stream* stream, ff_arena* arena, size_t initial_capacity);

// Closes the stream and leaves it unusable. Safe to call on an already-destroyed stream. A
// write-memory stream returns everything that was written (arena memory that outlives the stream);
// every other stream returns an empty span.
ff_span ff_stream_destroy(ff_stream* stream);

// Reads up to 'size' bytes from the current position, advancing it by the amount read. The returned
// span is shorter than 'size' at the end of the stream, and empty on failure. A read-memory stream
// returns a view into its source memory and ignores 'arena'; other streams allocate from 'arena'.
ff_span ff_stream_read_data(ff_stream* stream, ff_arena* arena, size_t size);

// Appends to a write stream. Returns false on failure or a partial write.
bool ff_stream_write_data(ff_stream* stream, ff_span data);

// Bytes available in a read stream, or bytes written so far to a write stream.
size_t ff_stream_size(const ff_stream* stream);

// Read position of a read stream, or bytes written so far to a write stream.
size_t ff_stream_pos(const ff_stream* stream);

// Moves the read position of a read stream, clamped to the size of the stream. Write streams only
// append, so seeking them fails.
bool ff_stream_seek(ff_stream* stream, size_t pos);
