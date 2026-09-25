#pragma once

// Registry of every live object that owns GPU state and has to be rebuilt when the device is
// recreated. This replaces the old C++ device_child_base virtual interface: instead of a vtable,
// each object embeds a node tagged with its type, and dx12_reset.c switches on that tag.
//
// Priority is a pure function of the type (it is the enum value), so it can never disagree with
// what the object actually is. before_reset runs in reverse priority order, while the old device
// is still alive, and reset runs in forward order against the new device. Ordering the enum so
// that owners come before the things they own means a resource frees its descriptor and memory
// ranges before the allocators holding them are touched.
//
// Globally-owned singletons (queues, the six mem allocators, the CPU/GPU descriptor allocators,
// residency) are not in this registry. dx12_reset.c drives them directly, in a fixed order it
// already knows, because they are created lazily by dx12_globals.c and there is exactly one of
// each. Only objects the caller can create an unbounded number of need to be tracked here.
typedef enum ff_dx12_device_child_type
{
    ff_dx12_device_child_type_resource,
    ff_dx12_device_child_type_buffer,
    ff_dx12_device_child_type_texture,
    ff_dx12_device_child_type_depth,
    ff_dx12_device_child_type_target_texture,
    ff_dx12_device_child_type_target_window,
    ff_dx12_device_child_type_object_cache,
    ff_dx12_device_child_type_count,
} ff_dx12_device_child_type;

// Embedded in each registered object. 'owner' points back at the containing struct, since the
// node is a member rather than a base class.
typedef struct ff_dx12_device_child
{
    struct ff_dx12_device_child* prev;
    struct ff_dx12_device_child* next;
    void* owner;
    ff_dx12_device_child_type type;
    bool pending_reset;
    bool registered;
} ff_dx12_device_child;

// Removing an unregistered node is harmless, so a destroy can call it unconditionally. Objects
// register in their init and remove in their destroy.
void ff_dx12_add_device_child(ff_dx12_device_child* child, void* owner, ff_dx12_device_child_type type);
void ff_dx12_remove_device_child(ff_dx12_device_child* child);

ff_dx12_device_child* ff_dx12_device_child_first(ff_dx12_device_child_type type);
ff_dx12_device_child* ff_dx12_device_child_next(ff_dx12_device_child* child);
size_t ff_dx12_device_child_count(ff_dx12_device_child_type type);

// True while a device reset is walking the registry.
bool ff_dx12_device_child_resetting(void);

// Walk support for dx12_reset.c. The walk tolerates children being added or removed by the
// callbacks it invokes: a removal moves the cursor forward off the dying node, and an addition
// is left unmarked so every pass skips it. A child created mid-reset was already built against
// whichever device is current, so resetting it would tear down state that was never stale.
void internal_ff_dx12_device_child_reset_begin(void);
void internal_ff_dx12_device_child_reset_end(void);
void internal_ff_dx12_device_child_walk_begin(ff_dx12_device_child_type type);
void internal_ff_dx12_device_child_walk_end(void);
ff_dx12_device_child* internal_ff_dx12_device_child_walk_next(void);
