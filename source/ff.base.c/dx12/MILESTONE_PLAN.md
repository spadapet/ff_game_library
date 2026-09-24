# DX12 pure-C port plan

Porting `source/ff.application/graphics/dx12/` (and `dxgi/`) into
`source/ff.base.c/dx12/` as pure C, following the `dx12_globals.c/h` style:
COBJMACROS COM calls, POD structs, explicit init/destroy, `ff_arena`
allocation, `ff_string_view` parameters.

## Roadmap (20 items, dependency order)

1. `dx12-math-types` - covered by existing `base/math.h` (`ff_math_round_up`,
   `ff_math_round_up_pow2`), no separate module needed.
2. `dx12-fence` - `ff_dx12_fence` / `ff_dx12_fence_value`.
3. `dx12-residency` - `ff_dx12_residency_data`, MRU/LRU list, `ff_dx12_make_resident`.
4. `dx12-heap` - `ff_dx12_heap`, usage enum, CPU mapping.
5. `dx12-mem-range` - `ff_dx12_mem_range`, tagged owner dispatch.
6. `dx12-mem-allocator` - ring + free-list buffers, outer growing allocator.
7. `dx12-descriptor-range` - `ff_dx12_descriptor_range`, explicit free.
8. `dx12-descriptor-allocator` - CPU free-list buckets + GPU pinned/ring.
9. `dx12-resource-state` - `ff_dx12_resource_state`, per-subresource states.
10. `dx12-resource-tracker` - `ff_dx12_resource_tracker`, barrier batching.
11. `dx12-resource` - `ff_dx12_resource` (placed/committed/external).
12. `dx12-queue` (future, milestone 3)
13. `dx12-commands` (future, milestone 3)
14. `dx12-object-cache` (future, milestone 3)
15. `dx12-buffer` (future, milestone 4)
16. `dx12-texture` (future, milestone 4)
17. `dx12-depth` (future, milestone 4)
18. `dx12-target` (future, milestone 4)
19. `dx12-shader-delivery` (future, milestone 5)
20. `dx12-draw-device` (future, milestone 5)

## Milestone 1 (complete)

Items 2-6 above: fence, residency, heap, mem_range, mem_allocator.

### Design decisions

- **fence_value is a raw pointer, not shared_ptr.** The old C++ used
  `std::shared_ptr<fence_wrapper_t>` so a `fence_value` could outlive its
  `fence`. In this single-threaded v1 port, `ff_dx12_fence_value` stores a
  raw `ff_dx12_fence*` directly. Fences are owned by long-lived objects
  (heaps, queues, allocators) that outlive any fence_value referencing them
  in every v1 use, so the indirection is dropped. Documented at the struct
  definition.
- **No `ff::dx12::queue` type exists yet.** `ff_dx12_fence_signal` /
  `ff_dx12_fence_wait` take a raw `ID3D12CommandQueue*` (may be NULL for a
  CPU-side signal/wait) instead of a queue wrapper.
- **Device-reset readiness, not full implementation.** Each module's
  `_init`/`_destroy` pair is written so `_destroy` then `_init` again on the
  same storage works (mirrors old `before_reset`/`reset`), but the global
  device_child registry/priority system is *not* built yet. No state is
  baked in that would make a later reset system structurally impossible.
- **Single-threaded v1, no mutexes.** Every spot where the old C++ took a
  mutex (`pageable_mutex`, `completed_value_mutex`, `ranges_mutex`,
  `buffers_mutex`) has a one-line comment noting the removed lock, so
  real threading support can find them later.
- **Arena-growth bounding strategy, per collection:**
  - `ff_dx12_fence_value` batch helpers, `ff_dx12_fence_values` (max 4
	entries), and residency's per-call scratch arrays (256 pageables) use
	fixed-capacity C arrays. These are bounded by frame-local batch sizes,
	not by run time or frame count.
  - The ring buffer's in-flight range bookkeeping uses a fixed-capacity
	circular buffer (64 entries) since it's naturally bounded by
	frames-in-flight.
  - The free-list buffer's free-range bookkeeping uses `ff_array` (arena
	growable) since fragmentation-bounded free lists are a reasonable use
	of a growable array - this is bounded by fragmentation/allocation
	count, not by elapsed time.
  - The outer `ff_dx12_mem_allocator`'s array of buffers also uses
	`ff_array`, matching old `std::vector<unique_ptr<mem_buffer_base>>`;
	bounded by heap-doubling count, not time.
- **Tagged structs, no vtables.** `ff_dx12_mem_buffer` is one struct with an
  enum discriminant (`ring` / `free_list`) and switch-based dispatch in
  each function, replacing the old virtual `mem_buffer_base` hierarchy.
  Same for `ff_dx12_mem_range`'s owner dispatch.

## Future milestones (3-5)

Milestone 3: command queues/lists, object cache.
Milestone 4: concrete buffer/texture/depth/target resource types.
Milestone 5: shader delivery + draw device (the high-level rendering API).

These are listed for roadmap completeness only; no implementation work has
started on them yet.

## Milestone 2 (complete)

Items 7-11 above: descriptor_range, descriptor_allocator, resource_state,
resource_tracker, resource.

### Design decisions

- **Descriptor buffers are one tagged struct.** `ff_dx12_descriptor_buffer`
  replaces the old `descriptor_buffer_base` / `_free_list` / `_ring` virtual
  hierarchy with an enum discriminant and switch dispatch, matching the
  `ff_dx12_mem_buffer` shape from milestone 1. A buffer describes a
  sub-range of a descriptor heap it does not own.
- **CPU buckets are a linked list, not an array.** A
  `ff_dx12_descriptor_range` stores a pointer to its owning buffer, so
  buckets must have stable addresses. They are arena-allocated nodes
  chained by `next` rather than elements of a relocatable `ff_array`.
- **Descriptor ring reuses the milestone-1 ring shape.** Fixed-capacity
  circular buffer of 64 in-flight ranges, bounded by frames-in-flight. The
  old C++ merged a new allocation into the last range whenever the fence
  values matched; the C version additionally requires the new start to be
  contiguous with that range, since after a wrap they are not adjacent and
  merging would have described the wrong descriptors.
- **`ff_dx12_resource_state` is copyable by value.** Storage is inline up
  to 8 subresources and spills into a caller-supplied arena beyond that.
  There is deliberately no pointer to the inline array, so the struct stays
  safe to assign and copy. `ff_dx12_resource_state_copy` deep-copies into a
  different arena when ownership has to move.
- **The tracker owns an arena it resets, never grows forever.**
  `ff_dx12_resource_tracker_reset` calls `ff_arena_reset` (not destroy) and
  rebuilds its arrays, so a tracker recycled every frame reuses its memory.
  All of its allocations - entry array, pending barriers, index map,
  per-entry state overflow - come from that one arena.
- **Pointer-keyed map replaces `std::unordered_map`.** An open-addressed
  array of `entries_a` indices (index + 1, 0 = empty), kept at most half
  full and rehashed on growth. `resource_moved` from the old C++ is not
  ported: resources are not move-constructed in C.
- **Tracker close deep-copies instead of swapping arrays.** The old C++
  swapped the resource maps between trackers. In C that would leave
  per-entry state pointing into another tracker's arena, so the merged
  state is copied back into this tracker's own arena and the previous
  tracker is reset.
- **Resource transfer operations are deferred to milestone 3.**
  `update_buffer` / `readback_buffer` / `capture_buffer` /
  `update_texture` / `readback_texture` / `capture_texture` all require the
  `commands` and `queue` types, so `ff_dx12_resource` currently covers
  creation (placed / committed / external), desc and subresource queries,
  global state and fence bookkeeping (`prepare_state`), residency, and
  view creation.
- **Resource kinds are an enum, not three classes.**
  `ff_dx12_resource_kind` distinguishes placed (owns a `mem_range`),
  committed (owns its own `residency_data`), and external (a swap chain
  buffer that is add-ref'd and never recreated).

## Review pass after milestone 2

A full read-through of all 13 dx12 modules before starting milestone 3
found three real bugs, all fixed with regression tests.

- **Dangling `ff_dx12_mem_range.owner` (milestone 1).** Buffers lived in an
  `ff_array`, but every range handed out points back at its owning buffer.
  Growing the array relocated it, and `frame_complete` shifted surviving
  buffers down, so live ranges pointed at freed or wrong buffers. Buffers
  are now an arena-allocated linked list with stable addresses, matching
  the descriptor allocator. Pruned nodes go on a `buffers_free` list so the
  arena doesn't grow each time a buffer is recycled.
- **`ff_dx12_cpu_descriptor_allocator_destroy` leaked descriptor heaps.**
  The loop read `bucket->next` after `descriptor_buffer_destroy` had zeroed
  the bucket, so it stopped after one bucket and leaked every other heap.
  The link is now read before the destroy; the same pattern was applied to
  the mem allocator's destroy loop.
- **`first_barriers` silently dropped barriers.** The legacy type was a
  `stack_vector<D3D12_RESOURCE_BARRIER, 4>` where 4 is the *inline*
  capacity, not a cap. The port used a fixed 4-element array plus an
  assert, so a first transition touching more than four subresources
  individually lost barriers (silently in release). It is now an
  arena-backed `ff_array` sized by the resource.
- **3D texture subresource count.** `ff_dx12_resource_array_size` returned
  `DepthOrArraySize`, which is depth (not array size) for 3D textures,
  giving the wrong subresource count. It now reports 1 for 3D resources.
- Smaller: `gpu_descriptor_allocator_init` ignored its buffer init results,
  and `descriptor_buffer_destroy` asserted on a never-initialized free list.

Still open (accepted for now): `descriptor_buffer_set_heap(NULL)` clears
`allocated_range_count` while ranges are outstanding, so their later free
asserts. That only happens on the device-reset path, which milestone 1
deliberately deferred.

## Second review pass: fixed-capacity arrays

Every fixed-size array was audited for what happens when it fills up. The
recurring bug shape was an `FF_ASSERT_RET*` on overflow. Those macros still
return in retail builds (only the assert dialog compiles out), so the early
return silently skips required work in a shipping build instead of failing
loudly the way the debug build does.

- `ff_dx12_fence_wait_value_array` returned from the whole function on the
  first NULL fence in the array, skipping every remaining wait. It now skips
  just that entry, matching the legacy C++ behavior.
- The same function asserted when more than `MAX_BATCH_FENCES` distinct fences
  were seen and abandoned the rest of the waits. It now flushes the batch it
  has gathered and starts a new one, so no wait is ever dropped.
- `ff_dx12_fence_values_add` dropped a fence value once the set was full. A
  dropped fence value is a missing GPU sync, i.e. a real race. The cap grew
  from 4 to 8 and overflow now over-synchronizes by blocking on the oldest
  entry to free a slot.
- `ff_dx12_make_resident`'s eviction loop returned mid-loop after already
  setting `data->resident = false`, leaving residency state inconsistent with
  the `Evict` call that never ran. It now stops batching before touching the
  entry.
- The descriptor ring asserted when its fixed range list filled. The ring can
  always reclaim by blocking, so it now drains the oldest ranges instead.
- The memory ring asserted in the same spot, but there "full" is a normal
  signal for the allocator to create another buffer, so it is now a plain
  early-out rather than an assert.
- `ff_dx12_heap_init` leaked the heap and left residency data registered when
  the CPU-side placed resource failed to create or map.

Checked and confirmed fine: `MAX_ADAPTERS` (bounded enumeration),
`FF_DX12_RESOURCE_STATE_INLINE_MAX` (has arena spill), the `wchar_t name[64]`
fields (truncation is cosmetic), and the `Texture2DArray` view paths (the
legacy copy/paste bug was not inherited).

## Third review pass: arithmetic and stale state

- `ff_dx12_make_resident` computed `available_space` as
  `Budget - CurrentUsage`. Unsigned subtraction wraps to a near-2^64 value
  whenever usage exceeds the budget, which is exactly the oversubscribed case,
  so the eviction loop's `delta_resident_size > available_space` condition was
  never true and eviction never ran when it was most needed. Now clamps to 0.
  This bug was inherited from the legacy C++ code.
- `ff_dx12_update_video_memory_info` never called `ResetEvent` on the
  budget-change event. It is a manual-reset event, so after the first budget
  change it stayed signaled forever and every later frame re-queried. The
  legacy code reset it; the port had dropped that.

Also confirmed correct on this pass: the free-list coalescing in both the
memory and descriptor allocators, `resource_state`'s inline/overflow
transitions, `prepare_state`'s read/write fence bookkeeping (matches legacy),
and `resource_tracker_close`'s deep-copy back into the tracker's own arena.

## Fourth review pass: line-by-line diff against the legacy C++

This pass compared each ported module statement-by-statement against its C++
original rather than reading the C in isolation. `resource_tracker`,
`resource_state`, `mem_allocator`, `descriptor_allocator`, `fence`, and
`residency` all came back behaviorally equivalent — no divergences found.

One real bug surfaced in `resource`:

- `ff_dx12_resource_destroy` released the `ID3D12Resource` and freed the
  `mem_range` immediately. The old code instead handed the resource to
  `keep_alive_resource` along with `global_reads` + `global_write`, so the
  release only happened once that GPU work retired. Destroying a resource
  while the GPU still had commands referencing it was therefore a GPU-side
  use-after-free, and for placed resources the `mem_range` could be handed to
  a new resource while still in flight. Destroy now blocks on those fences.
  The non-blocking keep-alive list needs the queue from milestone 3; this is
  the conservative interim behavior.

### REQUIRED milestone 3 follow-up: keep-alive list

Do not consider milestone 3 done until this lands. `ff_dx12_resource_destroy`
currently blocks the CPU, which is correct but stalls. The replacement:

- A global list of arena-allocated nodes, each capturing only the fields that
  need deferred release: `ID3D12Resource*`, `mem_range`, `residency_data`, plus
  the `ff_dx12_fence_values` guarding them. `ff_dx12_resource` is caller-owned
  POD, so the node cannot take ownership of the struct the way the C++ move did.
- `ff_dx12_keep_alive_resource` only queues when the fence values are
  incomplete; already-idle resources release immediately with no bookkeeping.
- `ff_dx12_flush_keep_alive` pops from the front and stops at the first
  incomplete entry, which is O(1) amortized because pushes are roughly in fence
  order.
- **Must be called from both `ff_dx12_frame_started` and
  `ff_dx12_wait_for_idle`.** An undrained list is an unbounded leak, so the
  drain call sites are not optional.

See `dx12_globals.cpp` `flush_keep_alive` (:156), `keep_alive_resource` (:662),
and the call sites at `frame_started` (:613) / `wait_for_idle` (:654).

## Fifth review pass: remaining modules diffed against the legacy C++

The fourth pass covered `resource_tracker`, `resource_state`, `mem_allocator`,
`descriptor_allocator`, `fence`, `residency`, and `resource`. This pass closed
the gap by diffing the modules that had never been rigorously compared:
`globals`, `heap`, `mem_range`, `descriptor_range`, and `fence_values`.

`heap`, `mem_range`, `descriptor_range`, and `fence_values` all came back
behaviorally equivalent. Every C++ function was matched to a C counterpart, the
manually-built D3D12 descriptors were checked field-by-field against the
`CD3DX12_*` helpers they replaced, and the `fence_value` -> `fence_values`
merge was confirmed to have lost no method.

One real bug surfaced in `globals`:

- `init_d3d` created the video-memory budget-change event *before* calling
  `ff_dx12_update_video_memory_info`. That event is manual-reset and starts
  unsignaled, so the update took the "no change yet" early-out and skipped the
  query entirely. `s_video_memory_info` stayed all zeros until the first budget
  change notification happened to fire. With `Budget == 0`, `available_space`
  in `ff_dx12_make_resident` is also 0, so residency evicted as aggressively as
  possible from the very first frame. The legacy queried first and registered
  the event second; the port now matches that order.

Covered by the existing `video_memory_info_has_a_budget` test, which asserts a
non-zero budget immediately after init.

## Milestone 3 (complete)

`dx12_queue`, `dx12_commands`, `dx12_object_cache`, plus the queues, frame
lifecycle, and keep-alive list in `dx12_globals`. 33 new tests; 792 pass.

Deliberate divergences from the legacy C++:

- **No thread pool.** The old code reset command lists on a `ff::thread_pool`
  task and gated cache hand-out on a `win_event`. The port resets inline right
  after `ExecuteCommandLists`, so the event, `wait_for_tasks()`, and every mutex
  disappear.
- **`ff_dx12_commands` is a handle, not an owner.** The `ff_dx12_command_cache`
  lives in queue-owned arena memory with a stable address; `commands` points at
  it. The old code moved a `std::unique_ptr` in and out.
- **Keep-alive doesn't defer `residency_data`.** `ff_dx12_residency_data` is an
  intrusive node in the global pageable list, so deferring it would leave a
  dangling node there. It's destroyed inline in `ff_dx12_resource_destroy`;
  that's safe because it only governs eviction, and the underlying pageable
  stays alive through the deferred `Release`.
- **`keep_alive_destroy` blocks.** At shutdown anything still queued has to be
  released, so it waits out each node's fence values.
- **The object cache is in-memory only.** The old code also persisted compiled
  pipelines to an on-disk `ID3D12PipelineLibrary` and resolved named shader
  blobs through the resource system. Both need shader delivery, so they land
  with that milestone.
- **`root_srv` uses a real `|`.** The C++ wrote
  `static_cast<D3D12_RESOURCE_STATES>(a, b)`, a comma expression that silently
  discards the first flag. Intentional bug fix.

## Sixth review pass: milestone 3 against the legacy C++

Four real bugs, all found by running the new tests rather than by reading:

- **Same-queue fence waits deadlocked.** Recording two state transitions on one
  resource in a single command list made `prepare_state` add that list's own
  fence to `wait_before_execute`, and `execute` then asked the queue to wait on
  a fence only that same queue could signal — from behind the wait. The GPU hung
  and the test host died. The legacy fence carried an owning `queue*` and
  skipped waits where `queue == this->queue_`; the port had no such field, with
  only a comment saying callers must not do this. `ff_dx12_fence` now has
  `owner_queue`, set by `ff_dx12_queue_init` for the idle fence and by
  `command_cache_acquire` for each cache fence, and both wait paths skip
  same-queue waits.
- **Residency silently dropped work past 256 pageables.** `ff_dx12_make_resident`
  had a fixed `MAX_RESIDENCY_BATCH` of 256 and returned `false` when the set was
  larger — and `false` makes `execute_many` skip `ExecuteCommandLists` entirely,
  so a command list touching 257+ resources was silently never submitted. The
  make-resident and evict lists are now arena-backed `ff_array`s with no cap.
- **`execute_many` capped at 32 commands and 256 residency entries.** Anything
  past the cap was dropped after an assert. The legacy used growable
  `stack_vector`s. All the scratch arrays now come from a stack-backed arena
  sized to the actual count.
- **Signaling many command lists blocked on an unsignaled fence.** `execute_many`
  gathered per-cache fence values into an `ff_dx12_fence_values`, which is
  fixed-capacity and, on overflow, CPU-blocks on its oldest entry to free a
  slot. Since every cache owns a distinct fence, a batch of more than 8 lists
  overflowed and blocked on a fence this very call hadn't signaled yet. Each
  value is now signaled directly; no dedup is needed because the fences are
  already distinct.

### REQUIRED milestone 4 follow-ups

Three legacy behaviors have no counterpart yet because they depend on types that
don't exist until milestone 4. None can be dropped:

- **`SetDescriptorHeaps` is never called.** The old `commands` constructor bound
  the GPU view and sampler heaps on every non-COPY list. The port has the
  `ff_dx12_gpu_descriptor_allocator` type but no global instances to bind, so
  nothing calls it. Shader-visible descriptor access and
  `SetGraphicsRootDescriptorTable` will not work until milestone 4 adds
  `ff_dx12_gpu_view_descriptors()` / `ff_dx12_gpu_sampler_descriptors()` and
  binds them from `command_cache_acquire` and `command_cache_reset_lists`.
- **`ff_dx12_commands_targets` transitions whole resources.** The legacy passed
  each target's `target_array_start/size` and `target_mip_start/mip_size`; the C
  API takes a bare `ff_dx12_resource*` and passes `0, 0, 0, 0`. Correct while a
  target is a whole resource, wrong once targets are slices or mips. The target
  wrapper type in milestone 4 must carry those four values through.
- **Upload-heap vertex/index buffers lose their residency.** When a buffer had
  no resource (CPU-mapped upload heap), the legacy called `keep_resident` on it.
  The C API takes `ff_dx12_resource**`, so that case can't be expressed and no
  residency is added. The buffer wrapper in milestone 4 needs to call
  `ff_dx12_commands_keep_resident` for the heap-backed case.

## Seventh review pass: array limits and arena lifetimes

An audit of every fixed-size array in the port, asking two questions per cap:
what happens on overflow, and which arena does the data live in for how long.

Two more deadlocks of the same family as the milestone 3 bugs, both found by
writing a test that forces the overflow path:

- **`ff_dx12_fence_values` blocked on an unsignaled fence (real deadlock,
  reproduced).** The legacy type was `ff::stack_vector<fence_value, 4>`, which
  spills to the heap and never blocks. The port made it a hard 8-entry array
  whose overflow path CPU-waits on `values[0]` to free a slot. That is fatal for
  `resource->global_reads`, which accumulates one *distinct* fence per command
  list that reads the resource: a resource read by 9 lists blocks inside
  `prepare_state` on a `signal_later` value that `execute_many` has not signaled
  yet and cannot signal, because it is still recording. One resource read by 9
  command lists hung the test host.
  The set is now inline-8 spilling into an arena, matching the `resource_state`
  pattern. It never blocks: it first drops entries the GPU has already passed
  (always safe, and enough in steady state), then grows. Every long-lived set
  got an arena whose lifetime covers it: `global_reads` and the destroy-time
  `pending` use the resource arena, `commands->wait_before_execute` and
  `execute_many`'s local use the queue and temp arenas, `residency_data`'s
  `keep_resident` uses the owning resource or heap arena (`ff_dx12_heap` gained
  an arena for this), and the keep-alive node *deep copies* rather than
  shallow-copying, since it outlives the resource arena it came from.

- **The GPU descriptor ring blocked on an unsignaled fence.** Same shape:
  `FF_DX12_DESCRIPTOR_RING_RANGES_MAX` was 64, and the overflow path waited on
  `ring_front` to reclaim a range slot. A frame submitting more than 64 command
  lists gives each range a distinct unsignaled fence, so that wait never
  returns. The range list now grows by doubling out of the allocator arena
  (`FF_DX12_DESCRIPTOR_RING_RANGES_MIN`). The *other* wait in that function is
  correct and stays: it reclaims descriptor space when the ring wraps, which
  genuinely does require the GPU to finish.

Caps that were checked and are fine, with the reason each is sound:

- `MAX_BATCH_FENCES` (16) in `wait_batch` — overflow *flushes* the batch
  gathered so far and starts a new one, so no wait is ever dropped.
- `FF_DX12_MEM_RING_RANGES_MAX` (64) — a full range list returns an invalid
  range, and `allocator_alloc_bytes` responds by creating another buffer. Never
  blocks, never drops.
- `FF_DX12_RESOURCE_STATE_INLINE_MAX` (8) — already spills to the resource arena
  via `reserve_entries`.
- `FF_DX12_RESIDENCY_SET_MIN` (256) — grows by doubling, kept ≤50% full.
- `MAX_ADAPTERS` (16) — a genuine hardware bound, and every adapter is released.
- Fixed `wchar_t name[64]` / `name[128]` buffers — all `_TRUNCATE`, and names are
  diagnostic only.

Arena growth was audited for unbounded accumulation. All the long-lived arenas
are bounded by a high-water mark rather than by run time: `queue->arena` recycles
allocator and cache nodes through free lists, `s_keep_alive_arena` recycles nodes
through `s_keep_alive_free`, the mem and descriptor allocators recycle buffers
through `buffers_free`, and `resource_tracker` resets its arena per frame. The
newly arena-backed fence value sets inherit the same property: they are reset by
`clear` and reuse their spilled block, so each one converges on the largest
number of distinct fences it has ever held.

## Bindless groundwork

The eventual bindless renderer (for better AMD performance) needs
`ResourceDescriptorHeap[]` indexing, which changes how descriptors are
addressed but not how they are allocated. Two pieces landed early so the
milestone 3 API doesn't have to change later:

- `ff_dx12_descriptor_range_heap_index` returns a descriptor's index within
  its whole heap, which is exactly the value a bindless shader indexes with.
- `ff_dx12_supports_bindless` reports resource binding tier 3 plus shader
  model 6.6, so the renderer can choose bindless or classic descriptor
  tables at init.

The GPU allocator's pinned free-list region is already the right shape for
a persistent bindless table; going bindless mainly means sizing it much
larger (order 1M CBV_SRV_UAV descriptors) and having views write into it
once instead of per-frame. Classic and bindless paths can share the same
allocator, so milestone 3 can proceed with normal descriptor tables.
