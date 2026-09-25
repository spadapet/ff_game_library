#include "pch.h"
#include "base/assert.h"
#include "base/log.h"
#include "dx12/dx12_buffer.h"
#include "dx12/dx12_depth.h"
#include "dx12/dx12_device_child.h"
#include "dx12/dx12_globals.h"
#include "dx12/dx12_object_cache.h"
#include "dx12/dx12_queue.h"
#include "dx12/dx12_reset.h"
#include "dx12/dx12_residency.h"
#include "dx12/dx12_resource.h"
#include "dx12/dx12_target_texture.h"
#include "dx12/dx12_target_window.h"
#include "dx12/dx12_texture.h"

static uint64_t s_reset_count;
static bool s_resetting;

static void child_before_reset(ff_dx12_device_child* child)
{
    switch (child->type)
    {
        case ff_dx12_device_child_type_resource:
            internal_ff_dx12_resource_before_reset((ff_dx12_resource*)child->owner);
            break;

        case ff_dx12_device_child_type_target_window:
            internal_ff_dx12_target_window_before_reset((ff_dx12_target_window*)child->owner);
            break;

        case ff_dx12_device_child_type_buffer:
            internal_ff_dx12_buffer_before_reset((ff_dx12_buffer*)child->owner);
            break;

        case ff_dx12_device_child_type_object_cache:
            internal_ff_dx12_object_cache_before_reset((ff_dx12_object_cache*)child->owner);
            break;

        // The rest own no GPU object of their own: their resource and their descriptor range are
        // both rebuilt in place by other passes, so there is nothing to release here.
        case ff_dx12_device_child_type_texture:
        case ff_dx12_device_child_type_depth:
        case ff_dx12_device_child_type_target_texture:
            break;

        default:
            FF_DEBUG_FAIL_MSG("unhandled device child type");
            break;
    }
}

static bool child_reset(ff_dx12_device_child* child, ff_dx12_commands* commands)
{
    switch (child->type)
    {
        case ff_dx12_device_child_type_resource:
            return internal_ff_dx12_resource_reset((ff_dx12_resource*)child->owner);

        case ff_dx12_device_child_type_buffer:
            return internal_ff_dx12_buffer_reset((ff_dx12_buffer*)child->owner, commands);

        case ff_dx12_device_child_type_texture:
            return internal_ff_dx12_texture_reset((ff_dx12_texture*)child->owner);

        case ff_dx12_device_child_type_depth:
            return internal_ff_dx12_depth_reset((ff_dx12_depth*)child->owner);

        case ff_dx12_device_child_type_target_texture:
            return internal_ff_dx12_target_texture_reset((ff_dx12_target_texture*)child->owner);

        case ff_dx12_device_child_type_target_window:
            return internal_ff_dx12_target_window_reset((ff_dx12_target_window*)child->owner);

        // Nothing to rebuild: the cache was emptied in before_reset and refills on demand against
        // the new device.
        case ff_dx12_device_child_type_object_cache:
            return true;

        default:
            FF_DEBUG_FAIL_MSG_RET_VAL("unhandled device child type", false);
    }
}

// True when the device itself has to be rebuilt. A stale DXGI factory is reported separately,
// because on its own it only means the adapter list may have changed: the factory is rebuilt and
// the new adapter hash decides whether the device goes with it.
static bool reset_needed(bool* out_dxgi_stale)
{
    *out_dxgi_stale = !ff_dx12_factory_current();

    return !ff_dx12_device_valid();
}

bool ff_dx12_reset_device(bool force)
{
    // Reentrancy would walk the registry twice and reset children that are mid-rebuild. This can
    // happen for real: a reset calls into code that reports a fatal error, which a caller might
    // answer with another reset.
    FF_ASSERT_RET_VAL(!s_resetting, false);

    bool dxgi_stale = false;
    const bool needed = reset_needed(&dxgi_stale);

    if (dxgi_stale)
    {
        const uint64_t old_hash = ff_dx12_adapters_hash();

        internal_ff_dx12_destroy_dxgi();

        if (!internal_ff_dx12_init_dxgi(true))
        {
            ff_log_write(ff_log_type_debug, FF_SVL("[dx12] Failed to recreate DXGI factory"));
            return false;
        }

        // A different set of adapters means the device may be running on hardware that is gone, or
        // that a better adapter appeared, so the device is rebuilt even if it still looks healthy.
        if (ff_dx12_adapters_hash() != old_hash)
        {
            ff_log_write(ff_log_type_debug, FF_SVL("[dx12] Adapters changed, forcing device reset"));
            force = true;
        }
    }

    // Nothing was actually wrong, so the device is left completely alone. Rebuilding a working
    // device would throw away every resource for no reason.
    FF_CHECK_RET_VAL(force || needed, true);

    ff_log_write(ff_log_type_debug, FF_SVL("[dx12] Resetting device"));

    // Every before_reset hook below releases GPU objects immediately rather than through the
    // keep-alive list, so the GPU has to be finished with them first. This is skipped when the
    // device is already lost: its queues can never signal again, so the wait would never return,
    // and the driver has already dropped the work.
    if (ff_dx12_device_valid())
    {
        ff_dx12_wait_for_idle();
    }

    s_resetting = true;
    internal_ff_dx12_device_child_reset_begin();

    // Teardown runs in reverse priority order, so an owner releases its references before the
    // thing it points at is torn down. target_window drops its back buffers before the resource
    // pass would otherwise try to rebuild them, and every resource releases its GPU object before
    // the heaps behind them go away.
    for (size_t i = ff_dx12_device_child_type_count; i-- > 0; )
    {
        internal_ff_dx12_device_child_walk_begin((ff_dx12_device_child_type)i);

        for (ff_dx12_device_child* child; (child = internal_ff_dx12_device_child_walk_next()) != NULL; )
        {
            child_before_reset(child);
        }

        internal_ff_dx12_device_child_walk_end();
    }

    internal_ff_dx12_allocators_before_reset();

    internal_ff_dx12_destroy_d3d(true);
    internal_ff_dx12_clear_fatal_error();

    bool result = internal_ff_dx12_init_d3d(true) && ff_dx12_residency_init();

    if (result)
    {
        result = internal_ff_dx12_allocators_reset();
    }

    if (!result)
    {
        ff_log_write(ff_log_type_debug, FF_SVL("[dx12] Device reset failed to recreate the device"));
        internal_ff_dx12_device_child_reset_end();
        s_resetting = false;
        return false;
    }

    // Buffers re-upload their contents, which needs a command list. It is opened once for the
    // whole walk and executed after it, so a reset costs a single submit no matter how many
    // buffers there are.
    ff_dx12_commands commands = { 0 };
    const bool has_commands = ff_dx12_queue_new_commands(ff_dx12_copy_queue(), &commands);

    for (size_t i = 0; i < ff_dx12_device_child_type_count; i++)
    {
        internal_ff_dx12_device_child_walk_begin((ff_dx12_device_child_type)i);

        for (ff_dx12_device_child* child; (child = internal_ff_dx12_device_child_walk_next()) != NULL; )
        {
            // A child that fails to rebuild is left invalid rather than aborting the walk: the
            // rest of the stack can still come back, and its owner sees it as invalid.
            result = child_reset(child, has_commands ? &commands : NULL) && result;
        }

        internal_ff_dx12_device_child_walk_end();
    }

    if (has_commands)
    {
        ff_dx12_queue_execute(ff_dx12_copy_queue(), &commands);
    }

    internal_ff_dx12_device_child_reset_end();
    s_resetting = false;
    s_reset_count++;

    ff_log_write(ff_log_type_debug, FF_SVL("[dx12] Device reset complete (%s)"),
        result ? "all children rebuilt" : "some children failed");

    return result;
}

uint64_t ff_dx12_device_reset_count(void)
{
    return s_reset_count;
}

void internal_ff_dx12_reset_shutdown(void)
{
    s_reset_count = 0;
    s_resetting = false;
}
