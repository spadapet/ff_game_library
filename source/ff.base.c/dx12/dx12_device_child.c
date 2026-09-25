#include "pch.h"
#include "base/assert.h"
#include "dx12/dx12_device_child.h"

static ff_dx12_device_child* s_heads[ff_dx12_device_child_type_count];
static ff_dx12_device_child* s_tails[ff_dx12_device_child_type_count];
static size_t s_counts[ff_dx12_device_child_type_count];

// True between reset_begin and reset_end. A child created while a reset is in progress is left
// with pending_reset clear, so every pass skips it: it was already built against the new device,
// and resetting it would tear down state that was never stale.
static bool s_resetting;

// The node the current pass is about to visit. If that exact node is removed between callbacks,
// the cursor has to move off it before its links are cleared.
static ff_dx12_device_child* s_walk_cursor;

void internal_ff_dx12_device_child_reset_begin(void)
{
    for (size_t i = 0; i < ff_dx12_device_child_type_count; i++)
    {
        for (ff_dx12_device_child* child = s_heads[i]; child; child = child->next)
        {
            child->pending_reset = true;
        }
    }

    s_resetting = true;
}

void internal_ff_dx12_device_child_reset_end(void)
{
    for (size_t i = 0; i < ff_dx12_device_child_type_count; i++)
    {
        for (ff_dx12_device_child* child = s_heads[i]; child; child = child->next)
        {
            child->pending_reset = false;
        }
    }

    s_resetting = false;
    s_walk_cursor = NULL;
}

void internal_ff_dx12_device_child_walk_begin(ff_dx12_device_child_type type)
{
    FF_ASSERT_RET(type < ff_dx12_device_child_type_count);
    s_walk_cursor = s_heads[type];
}

void internal_ff_dx12_device_child_walk_end(void)
{
    s_walk_cursor = NULL;
}

ff_dx12_device_child* internal_ff_dx12_device_child_walk_next(void)
{
    while (s_walk_cursor)
    {
        ff_dx12_device_child* child = s_walk_cursor;
        s_walk_cursor = child->next;

        if (child->pending_reset)
        {
            return child;
        }
    }

    return NULL;
}

void ff_dx12_add_device_child(ff_dx12_device_child* child, void* owner, ff_dx12_device_child_type type)
{
    FF_ASSERT_RET(child && owner && type < ff_dx12_device_child_type_count);
    FF_ASSERT_RET(!child->registered);

    child->owner = owner;
    child->type = type;
    child->registered = true;
    child->pending_reset = false;
    child->next = NULL;
    child->prev = s_tails[type];

    if (s_tails[type])
    {
        s_tails[type]->next = child;
    }
    else
    {
        s_heads[type] = child;
    }

    s_tails[type] = child;
    s_counts[type]++;
}

void ff_dx12_remove_device_child(ff_dx12_device_child* child)
{
    FF_ASSERT_RET(child);

    if (!child->registered)
    {
        return;
    }

    if (s_walk_cursor == child)
    {
        s_walk_cursor = child->next;
    }

    if (child->prev)
    {
        child->prev->next = child->next;
    }
    else
    {
        s_heads[child->type] = child->next;
    }

    if (child->next)
    {
        child->next->prev = child->prev;
    }
    else
    {
        s_tails[child->type] = child->prev;
    }

    s_counts[child->type]--;

    child->prev = NULL;
    child->next = NULL;
    child->owner = NULL;
    child->pending_reset = false;
    child->registered = false;
}

bool ff_dx12_device_child_resetting(void)
{
    return s_resetting;
}

ff_dx12_device_child* ff_dx12_device_child_first(ff_dx12_device_child_type type)
{
    FF_ASSERT_RET_VAL(type < ff_dx12_device_child_type_count, NULL);
    return s_heads[type];
}

ff_dx12_device_child* ff_dx12_device_child_next(ff_dx12_device_child* child)
{
    return child ? child->next : NULL;
}

size_t ff_dx12_device_child_count(ff_dx12_device_child_type type)
{
    FF_ASSERT_RET_VAL(type < ff_dx12_device_child_type_count, 0);
    return s_counts[type];
}
