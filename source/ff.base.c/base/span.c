#include "pch.h"
#include "base/span.h"

ff_span ff_span_empty(void)
{
    return (ff_span) { 0 };
}

ff_array_span ff_array_span_empty(void)
{
    return (ff_array_span) { 0 };
}

ff_array_slice ff_array_slice_empty(void)
{
    return (ff_array_slice) { 0 };
}
