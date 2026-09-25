# DX12 pure-C port plan

Porting `source/ff.application/graphics/dx12/` (and `dxgi/`) into
`source/ff.base.c/dx12/` as pure C, following the `dx12_globals.c/h` style:
COBJMACROS COM calls, POD structs, explicit init/destroy, `ff_arena`
allocation, `ff_string_view` parameters.

## Roadmap (24 items, dependency order)

Items 1-19 are complete. Items 20-24 are milestone 6 and later.

The roadmap was re-checked against the old `source/ff.application/graphics/`
tree after device reset landed, because the original 20 items were written from
the `dx12/` folder alone and missed things that live in `dxgi/`, `types/`, and
`resource/`. What came out of that comparison:

- `dx12/texture_view` was genuinely missing and is now item 19. Sprites need
  sub-range views, so it is a prerequisite for the renderer rather than
  something to defer.
- `dxgi/format_util`, `dx12/gpu_event`, and `types/color` are real but small.
  They get pulled in as the code that needs them is written, not as milestones
  of their own.
- `dxgi/sprite_data` is deliberately left as late as possible.
- Palette support is required eventually, so `dxgi/palette_base` and
  `resource/palette_data` / `palette_cycle` get pulled in when the renderer
  reaches the palette path.
- Out of scope for now, and not on the roadmap at all: `resource/animation*`,
  `sprite_font`, `sprite_optimizer`, `png_image`, and all of `write/`
  (DirectWrite). These are content and text layers, not the graphics engine.

1. `dx12-math-types` - covered by existing `base/math.h` (`ff_math_round_up`,
   `ff_math_round_up_pow2`), no separate module needed. (complete)
2. `dx12-fence` - `ff_dx12_fence` / `ff_dx12_fence_value`. (complete)
3. `dx12-residency` - `ff_dx12_residency_data`, MRU/LRU list, `ff_dx12_make_resident`. (complete)
4. `dx12-heap` - `ff_dx12_heap`, usage enum, CPU mapping. (complete)
5. `dx12-mem-range` - `ff_dx12_mem_range`, tagged owner dispatch. (complete)
6. `dx12-mem-allocator` - ring + free-list buffers, outer growing allocator. (complete)
7. `dx12-descriptor-range` - `ff_dx12_descriptor_range`, explicit free. (complete)
8. `dx12-descriptor-allocator` - CPU free-list buckets + GPU pinned/ring. (complete)
9. `dx12-resource-state` - `ff_dx12_resource_state`, per-subresource states. (complete)
10. `dx12-resource-tracker` - `ff_dx12_resource_tracker`, barrier batching. (complete)
11. `dx12-resource` - `ff_dx12_resource` (placed/committed/external). (complete)
12. `dx12-queue` - `ff_dx12_queue`, allocator/list pooling. (complete, milestone 3)
13. `dx12-commands` - `ff_dx12_commands`. (complete, milestone 3)
14. `dx12-object-cache` - root signatures, PSOs. (complete, milestone 3)
15. `dx12-buffer` - `ff_dx12_buffer`. (complete, milestone 4)
16. `dx12-texture` - `ff_dx12_texture`. (complete, milestone 4)
17. `dx12-depth` - `ff_dx12_depth`. (complete, milestone 4)
18. `dx12-target` - `ff_dx12_target_texture`. (complete, milestone 4)
19. `dx12-texture-view` - `ff_dx12_texture_view`, a sub-range SRV over an
    `ff_dx12_texture`. (complete, milestone 6)
20. `png-decode` - simple libpng decoding into an arena, no DirectXTex.
    Milestone 6.
21. `dx12-math-types` - `ff_color`, `ff_matrix`, `ff_matrix_stack`,
    `ff_transform`, `ff_viewport` in plain C. Milestone 6.
22. `dx12-shader-delivery` - milestone 6.
23. `dx12-draw-device-state` - root signatures, PSO permutations, constant
    buffers, and the state stacks. Milestone 6.
24. `dx12-draw-device-batching` - instance buckets, transparency ordering, and
    the `draw_*` entry points. Milestone 7.

## Milestone status

- Milestone 1: fence, residency, heap, mem_range, mem_allocator. (complete)
- Milestone 2: descriptor_range/allocator, resource_state, resource_tracker,
  resource. (complete)
- Milestone 3: command queues/lists, object cache. (complete)
- Milestone 4: concrete buffer/texture/depth/target resource types. (complete)
- Milestone 5: swap chain / `target_window`. (complete; image decoding for
  textures deferred to a later milestone)
- Device reset: `dx12_device_child` registry + `dx12_reset`. (complete)
- Milestone 6: texture views, math types, shader delivery, and the draw
  device's state layer (root signatures, PSOs, constants).
- Milestone 7: the batching renderer on top of milestone 6, then bindless.

Twelve review passes have been run against the completed milestones; each is
recorded below. Full suite: 894 passing, zero skipped. See "Current state and
next steps" at the end for what remains.

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
- **Arena-growth bounding strategy, per collection.** Several of these were
  revised by later review passes; the current state is:
  - `ff_dx12_fence_value` batch helpers and residency's per-call scratch
	arrays (256 pageables) use fixed-capacity C arrays, bounded by
	frame-local batch sizes rather than by run time or frame count.
  - `ff_dx12_fence_values` was originally a fixed 4- then 8-entry array. The
	second and eighth review passes showed that overflowing it caused a real
	deadlock, so it is now inline-8 spilling into an arena and never blocks.
  - The ring buffer's in-flight range bookkeeping uses a fixed-capacity
	circular buffer (64 entries) since it's naturally bounded by
	frames-in-flight. Overflow returns an invalid range, which makes the
	outer allocator create another buffer; it never blocks or drops work.
  - The free-list buffer's free-range bookkeeping uses `ff_array` (arena
	growable) since fragmentation-bounded free lists are a reasonable use
	of a growable array - this is bounded by fragmentation/allocation
	count, not by elapsed time.
  - The outer `ff_dx12_mem_allocator`'s buffers were an `ff_array`, but
	`ff_dx12_mem_range` points back at its owning buffer, so a relocating
	array was unsound. They are now an arena-allocated intrusive linked list
	(`buffers` / `buffers_free` / `buffers_count`) with stable addresses,
	ordered newest-first and pruned on `frame_complete`.
- **Tagged structs, no vtables.** `ff_dx12_mem_buffer` is one struct with an
  enum discriminant (`ring` / `free_list`) and switch-based dispatch in
  each function, replacing the old virtual `mem_buffer_base` hierarchy.
  Same for `ff_dx12_mem_range`'s owner dispatch.

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

Resolved by the reset-review pass below: `descriptor_buffer_set_heap(NULL)` and
the mem allocator's `before_reset` both clear ring bookkeeping while ranges are
outstanding. The only range that actually outlived a reset was a buffer's
`mapped_range`, which `internal_ff_dx12_buffer_before_reset` now drops.

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
  a new resource while still in flight. Destroy blocked on those fences as an
  interim fix; the non-blocking keep-alive list replaced that in milestone 3.

### REQUIRED milestone 3 follow-up: keep-alive list (done)

Landed in milestone 3. `ff_dx12_resource_destroy` no longer blocks. The design
as built:

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

Both drain call sites exist in the C port (`ff_dx12_frame_started` and
`ff_dx12_wait_for_idle` in `dx12_globals.c`). Note that `keep_alive_destroy` is
a third, different case: it runs after the queues that own the fences are
destroyed, so it must release without consulting fence values at all. See the
twelfth review pass.

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

### REQUIRED milestone 4 follow-ups (all resolved in milestone 4)

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

## Eighth review pass: timing, deadlocks, and missing functionality

A full inventory of every CPU-blocking wait site in the dx12 sources, plus a
function-by-function diff of every ported module against the legacy C++.

### A fence value that nobody ever signals (fixed)

`ff_dx12_make_resident` takes `resident_fence_value` from
`ff_dx12_fence_signal_later(&s_residency_fence)` and stores it in
`data->resident_value` for every newly resident allocation. That value is only
ever signaled on the GPU by `ID3D12Device3::EnqueueMakeResident`. On the
fallback path, where the device does not expose `ID3D12Device3` and the code
calls the synchronous `ID3D12Device6::MakeResident` instead, no signal was ever
issued, so the value stayed permanently incomplete.

The hang shows up on a *later* call. That call sees the data already resident
with an incomplete `resident_value`, decides it is "still becoming resident
from a different call", and adds the dead value to `wait_values`.
`ff_dx12_queue_execute_many` then waits on it with the command queue, which
blocks the queue forever on a signal that has no source.

`s_residency_fence` is now signaled directly on the CPU after a successful
synchronous `MakeResident`, which matches the fact that the residency work is
already finished by the time that call returns. The legacy C++ has the same
hole; this is a fix rather than a port error.

### Verified sound

The other wait inside `make_resident`, on `wait_to_evict`, is safe. The
eviction loop only visits data whose `usage_counter` is not the counter just
assigned, so nothing in the current residency set can contribute, and
`commands_fence_value` is only added to `keep_resident` after the wait. A
later call that evicts that data can block on the caller's command fence, but
that fence is signaled by an `execute_many` that has already submitted, so it
always makes progress.

A separate audit compared every public function in the ported legacy modules
(fence, fence values, heap, mem allocator and range, descriptor allocator and
range, residency, resource, resource state, resource tracker, globals, queue,
queues, commands, object cache) against its C counterpart. No ported function
drops a fence signal or wait, skips a residency or state bookkeeping step, or
reorders side effects. The only absent legacy entry points are
`resource_tracker::resource_moved`, which has no meaning now that resources are
non-movable structs, and the `resource::update_buffer` / `readback_buffer` /
`update_texture` / `readback_texture` convenience wrappers, whose underlying
`commands` primitives are all present and which belong with the buffer and
texture wrapper types in a later milestone.

## Ninth review pass: resource state transitions

A line-by-line comparison of `dx12_resource_state.c` and `dx12_resource_tracker.c`
against `resource_state.cpp` and `resource_tracker.cpp`, plus the arena and array
growth paths underneath them.

### The transition logic is faithful

`allow_promotion`, `allow_decay`, and `needs_transition` match the legacy
predicates exactly, including the "common to a single write state" power-of-two
check and the copy-queue and simultaneous-access decay shortcuts. `set`, `get`,
`merge`, `all_same`, and the `assert_type_change` table are equivalent, and the
deferred `check_all_same` collapse behaves the same as the old
`std::vector::resize(1)`. `close` resolves first barriers, splits an
`ALL_SUBRESOURCES` barrier when the previous state has diverged, merges forward,
and decays into global state in the same order as the original.

The C version also differs harmlessly in one spot: the old `close` patched
`StateBefore` and `Subresource` directly on the `first_barriers` entry through a
reference, while the port copies into a local `resolved` first. Both are correct,
since every iteration reassigns both fields before pushing and the list is
cleared immediately afterward, but the copy makes the split case easier to follow.

### Growth paths are sound

`ff_arena_declare_stack` passes `grow_buffer_size` of 0, which
`ff_arena_init_external` treats as "default to the external size" rather than
"never grow", so the 1024 byte barrier arena in `close` spills to the heap
instead of failing. Confirmed by resolving 200 barriers through one `close`.
`ff_array_push` doubles through `ff_arena_realloc`, and the inline-8 state
storage spills into the owning arena, so neither has a fixed ceiling.

### Test coverage gap closed

`ff_dx12_resource_tracker_close` had no direct coverage at all, which is
notable given it holds the subtlest logic in the module. Three tests now drive
it through a real execute: 200 resolved barriers in one close, 16 divergent
subresources spilling past inline storage across two command lists, and a
whole-resource barrier splitting against a divergent previous state.

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
allocator, so the classic renderer can proceed with normal descriptor tables.

## Milestone 4 (complete)

Concrete wrapper types, all 8 files added to the vcxproj/filters and the 4
public headers to `include/ff.base.c.h`.

Scope decision: `ff_dx12_texture` is GPU-only. The legacy texture depends on
DirectXTex `ScratchImage` for CPU-side image data and mip generation, and
ff.base.c has no image codec, so image decoding moves to milestone 5 along
with `target_window` / the swap chain. Uploads go through
`ff_dx12_texture_update` / `ff_dx12_commands_update_texture` instead.

Landed first, because every milestone 4 type depends on them and they had
never been ported:

- The 12 global allocators in `dx12_globals.c` (6 mem, 6 descriptor), created
  lazily, with sizes matching the legacy exactly. `frame_complete` is called
  on them directly from `ff_dx12_frame_complete` instead of through a signal.
- `destroy_allocators()` sits in `destroy_d3d` after the keep-alive drain and
  before `ff_dx12_residency_destroy`. That ordering is load-bearing: heaps back
  deferred releases, and every heap registers residency data.
- `ff_dx12_fix_sample_count`. The legacy halving loop never updates
  `levels.SampleCount`, so it re-tests the same value forever; the C version
  rebuilds the feature-data struct each iteration.

Two required follow-ups from the milestone 3 review also landed:
`SetDescriptorHeaps` for non-COPY lists in `ff_dx12_queue_new_commands`, and
target sub-ranges via `ff_dx12_target_range` on `ff_dx12_commands_targets`.
The third (upload-heap residency for vertex/index buffers) is moot in the C
design: every `ff_dx12_buffer` kind owns a real `ff_dx12_resource`, and
`ff_dx12_commands_update_buffer` already calls `keep_resident` on the upload
range, with `ff_dx12_buffer_residency_data` available for view callers.

Types: `ff_dx12_depth`, `ff_dx12_texture`, `ff_dx12_target_texture` (borrows
its texture; the texture must outlive it), and `ff_dx12_buffer` (one tagged
struct with `gpu_static` / `gpu` / `cpu` kinds).

`ff_dx12_buffer_update` keeps the legacy hash-skip with the 0x10000 cutoff but
adds a size check to the skip condition, since the legacy compared the hash
alone. Growth doubles and does not preserve old contents, because `map`
overwrites them anyway.

### Bugs found by the milestone 4 tests

1. Ring mem ranges are retired by fence and never explicitly freed, so
   `allocated_range_count` was always non-zero at teardown and tripped the leak
   assert in `ff_dx12_mem_buffer_destroy`. `ff_dx12_mem_allocator_destroy` now
   clears the ring bookkeeping outright. It must not call `frame_complete` to
   do this: queues are destroyed before allocators, so the fences are already
   gone.

2. New pattern: intrusive-node struct copy. `ff_dx12_resource` embeds an
   `ff_dx12_residency_data` node that lives in a global doubly-linked list, so
   building a resource in a local and assigning it into its final home leaves
   the list pointing at the dead local. Always `destroy` then `init` in place.
   This bit `dx12_buffer.c` and `dx12_depth.c`; the symptom was the
   `!s_pageable_front` assert firing far away in `ff_dx12_residency_destroy`.

3. Destroying a resource while a command list still referenced it (any buffer
   or depth resize mid-frame) tripped the `!resource->tracker` assert and left
   the tracker holding a dangling wrapper pointer. Added
   `ff_dx12_resource_tracker_forget`, called from `ff_dx12_resource_destroy`.
   Only the tracker's pointer back to the wrapper has to go: the recorded
   barriers name the `ID3D12Resource`, which outlives this via keep-alive.

20 milestone 4 tests; full suite at 818 passing, zero skipped.

## Tenth review pass: GPU lifetime, ordering, and mid-frame CPU blocking

Driven by three questions: can memory ever be released while the GPU is using
it, does anything block the CPU on the GPU mid-frame, and is the legacy
thread-pool command list reset still needed.

### The unsubmitted-fence deadlock class

`ff_dx12_fence_signal_later` reserves a fence value without submitting it.
Nothing signals that value until the owning command list is executed, so a CPU
block on it can never be satisfied. Three places blocked on values that could
be in that state.

`ff_dx12_fence` now tracks `signaled_value`, and
`ff_dx12_fence_value_wait_is_pending` / `ff_dx12_fence_values_wait_is_pending`
report whether a CPU wait can ever complete. Fixed sites:

- `ff_dx12_descriptor_buffer_alloc_ring` blocked to reclaim ring space held by
  a range whose fence had only been reserved. Now it fails the allocation
  quietly (out of room is a normal condition) instead of hanging. This matches
  what the mem ring allocator already did.
- Residency eviction blocked on a pageable's `keep_resident` values. Candidates
  whose values aren't submitted are now skipped, leaving them resident and
  moving further up the LRU list rather than hanging.
- The keep-alive fallback (only reachable on arena allocation failure) blocked
  before releasing. It now leaks rather than hangs when the values aren't
  submitted.

### Use-after-free: residency data outliving its resource

`ff_dx12_residency_data` is embedded in `ff_dx12_resource`, and every command
list that touches a resource stores a raw pointer to it in its residency set.
Destroying a resource mid-recording left those sets pointing into freed memory,
which `ff_dx12_make_resident` then dereferenced at execute time. This crashed
the test host in two new tests.

Fixed with `ff_dx12_residency_set_remove` /
`ff_dx12_queue_forget_residency_data` / `ff_dx12_forget_residency_data`, called
from `ff_dx12_residency_data_destroy`. Removal reinserts the rest of the probe
run, since clearing a slot in an open-addressed table breaks the chain behind
it. This required a `caches_in_use` list on the queue: caches handed out to a
live `ff_dx12_commands` were previously unreachable from the queue.

This is the same shape as the milestone 4 tracker bug. The general rule: any
raw pointer *into* an `ff_dx12_resource` held by a command list must be
scrubbed when the resource dies, because only the `ID3D12Resource` itself is
kept alive by the keep-alive list.

### Descriptor ring leak assert

`ff_dx12_descriptor_buffer_destroy` asserted `allocated_range_count == 0`, but
descriptor ring ranges are retired by fence and never explicitly freed, exactly
like the mem ring. The assert is now replaced by clearing the bookkeeping.

### Is the thread-pool command list reset still needed?

No, not at present. The legacy `queue::execute` posted allocator and list
`Reset` to `ff::thread_pool` and gated hand-out on a per-cache event, with
`wait_for_tasks` draining it. The C port resets inline.

Measured in Release with `measure_command_list_reset_cost`: the whole
`ff_dx12_queue_execute` (close, residency, ExecuteCommandLists, signal, and
both `Reset` calls) averages **6.8 us**, worst case 47.8 us over 256
iterations. That is far below a frame budget, so moving it off the render
thread would add synchronization for no gain.

Worth revisiting if either becomes true: many command lists are executed per
frame (cost is per list), or allocators start holding large recorded lists,
since `ID3D12CommandAllocator::Reset` cost scales with what was recorded into
it. `windows/task.h` already provides the thread pool if it is needed later.

### New tests

`dx12_lifetime_tests` (14 tests) covers resource destroy while recording,
repeated resize mid-recording for buffers and depth, destroy before execute,
64 buffers destroyed in one list, allocator recycling across 32 executes,
upload ring reuse across 48 frames, two lists sharing a resource,
textures destroyed mid-recording, frame_complete ordering, and device destroy
with work still in flight. `dx12_reset_timing_tests` measures the reset cost.

Full suite: 832 passing, zero skipped.

## Eleventh review pass

Targeted at destroy paths whose asserts can skip real cleanup, and at the "raw pointer into a
dying resource" pattern that produced the last three bugs.

### Ring buffers pruned mid-run leaked a heap (fixed)

`ff_dx12_mem_allocator_frame_complete` prunes a ring `ff_dx12_mem_buffer` once its ranges retire,
but upload callers never call `free_range`, so `allocated_range_count` is normally non-zero.
`ff_dx12_mem_buffer_destroy` asserted that count was zero and returned early through the assert
handler, skipping `ff_dx12_heap_destroy`. The heap's residency node stayed in the global pageable
list, and teardown then tripped the `!s_pageable_front && !s_pageable_back` assert in
`ff_dx12_residency_destroy`.

Ring ranges retire by fence rather than by an explicit free, so the count is expected state, not a
leak. `ff_dx12_mem_buffer_destroy` now clears the ring bookkeeping itself. The duplicate clearing
loop in `ff_dx12_mem_allocator_destroy` was removed as redundant. This mirrors the same fix already
applied to `ff_dx12_descriptor_buffer_destroy`.

This only reproduced once the test was strengthened to assert `buffer_count > 1` and
`outstanding > 0`; the original version allocated against already-signaled fences, so the ring
reused a single heap and never pruned. A passing test that never reaches the code it names is
worse than no test.

Full suite: 844 passing, zero skipped.

## Twelfth review pass

### keep_alive_destroy consulted fences that were already destroyed (fixed)

`destroy_d3d` runs `wait_for_idle`, then destroys the three queues, then calls
`keep_alive_destroy`. That last function looped over every queued node and called
`ff_dx12_fence_values_wait` on it, which reads through `ff_dx12_fence_value::fence` into fences
that the queue destroy had already torn down.

Two separate problems in one line:

1. A use-after-free on the fence objects, since the queues own them and are gone by then.
2. A node can name a `signal_later` value that was never submitted, when a command list records a
   barrier against a resource and is then never executed. `ff_dx12_resource_destroy` hands
   `global_write` to the keep-alive list, so that unsatisfiable value ends up in a node. Waiting on
   it could never return.

`wait_for_idle` already ran, so the GPU is idle and nothing in the list is still referenced. The
loop now releases every node outright without consulting any fence.

An intermediate attempt that guarded with `ff_dx12_fence_values_wait_is_pending` was wrong for the
same underlying reason: it still dereferences the destroyed fence. At this point in teardown the
fence values carry no usable information at all.

Covered by three ordering tests in `dx12_lifetime_deep_tests`: list destroyed before the resource,
resource destroyed before the list, and a list that is never executed at all (the reproducer).

Full suite: 847 passing, zero skipped.

## Milestone 5: swap chain (`ff_dx12_target_window`)

Implemented in `dx12_target_window.h` / `dx12_target_window.c`, covered by ten
tests in `test/ff.test.unit.c/dx12/dx12_target_window_tests.cpp`. Full suite:
857 passing, zero skipped.

### Scope

The milestone was narrowed to the swap chain alone. Image decoding for
`ff_dx12_texture` is a genuinely separable piece with no shared code (ff.base.c
still has no image codec; WIC is the natural Win32-only replacement for the
legacy DirectXTex `ScratchImage`, including mip generation), and the swap chain
is the critical path to putting anything on screen. Decoding moves to a later
milestone.

### What was built

- `ff_dx12_target_window`: `IDXGISwapChain4` created with
  `CreateSwapChainForHwnd` on the direct command queue, `FLIP_DISCARD`,
  `FF_DX12_TARGET_WINDOW_BUFFER_COUNT` (2) back buffers, and
  `FRAME_LATENCY_WAITABLE_OBJECT`.
- Each back buffer is an `ff_dx12_resource` built with
  `ff_dx12_resource_init_external`, so the existing state tracker, residency and
  barrier machinery apply to them unchanged. RTVs come from the shared
  `ff_dx12_cpu_target_descriptors` allocator.
- `MakeWindowAssociation(..., DXGI_MWA_NO_WINDOW_CHANGES)`. Full screen in this
  library is borderless and driven by window style in
  `source/ff.base.c/windows/window.c`, not DXGI exclusive full screen, so DXGI
  must never react to alt-enter itself.
- Frame pacing ported faithfully from the legacy ladder: four stages
  `{latency 1, vsync}, {1, no vsync}, {2, vsync}, {2, no vsync}`, EMA over a
  window of 16 with alpha 1/16, good/bad thresholds 58/54 fps, two good windows
  to improve, one window ignored after a resize. Pacing resets on resize, and a
  latency change re-acquires the frame-latency waitable handle.

### The load-bearing ordering constraint: resize vs. the keep-alive list

`IDXGISwapChain::ResizeBuffers` fails unless every reference to the back buffers
has been released. But `ff_dx12_resource_destroy` does not release the
`ID3D12Resource`; it queues the release onto the global keep-alive list, which
is only drained by `ff_dx12_flush_keep_alive`. So `set_size` must call
`ff_dx12_wait_for_idle()` (which retires GPU work *and* flushes keep-alive)
**before** `destroy_back_buffers`. Without that drain the debug layer reports a
live-reference error, which with `SetBreakOnSeverity(ERROR)` kills the test host
outright.

This is the swap chain's instance of the general rule that deferred release is
invisible at the call site, and it is the reason resize is the only operation
here that blocks the CPU on the GPU. It happens at a frame boundary during an
explicit resize, never mid-frame.

The frame-latency wait in `end_render` runs *before* `ExecuteCommandLists` and
`Present`, so it paces the CPU at the frame boundary rather than stalling in the
middle of building a frame.

### Bugs found by self-review before the code was committed

- `create_back_buffers` partial failure destroyed uninitialized entries.
  Replaced the `bool back_buffers_valid` flag with a `size_t
  back_buffers_created` count, incremented only after a successful init, so the
  error path and `destroy` both unwind exactly what exists.
- `ff_dx12_target_window_valid` accepted a partial back-buffer count; it now
  requires the full count.
- `set_size` early-returned `true` whenever the requested size matched the
  current size, which was wrong if an earlier failure had left the buffers
  missing at that same size. It now also requires the object to be fully built.

### Bug found by a subsequent review pass

`end_render` discarded the result of `update_pacing`, which calls
`apply_latency`, which can call `ff_dx12_device_fatal_error` when
`SetMaximumFrameLatency` fails. A device-fatal failure raised through that path
was therefore reported to the caller as a successful present, delaying the
device rebuild indefinitely. `update_pacing` now returns `bool`, and
`end_render` returns false if pacing failed, if `Present` returned
`DEVICE_RESET`/`DEVICE_REMOVED`, or if `ff_dx12_device_valid()` has gone false
for any other reason.

### Verifying the tests are not vacuous

The resize drain was fault-injected by removing the `wait_for_idle` call.
`present_then_resize_repeatedly` failed as expected, but
`resize_releases_back_buffers` still passed, because it never rendered and so
had nothing pending in keep-alive. It was strengthened to render before each
resize; both now fail without the drain and pass with it. This technique has
now caught two real gaps (the eleventh-pass ring prune and this one) and should
be applied to every test that guards an ordering constraint.

### Not ported

- Window-message-driven resize (`WM_SIZE`, `WM_ENTERSIZEMOVE`,
  `WM_EXITSIZEMOVE` deferring the resize until the drag ends). The C port
  exposes `set_size` and expects the caller to drive it; connecting that to
  `ff_window`'s signals is a follow-up.
- Display rotation. The legacy called `IDXGISwapChain::SetRotation` from a
  `rotation` field on its size type. `ff.base.c`'s window layer has no rotation
  concept at all, so there is nothing to plumb through yet; rotated displays get
  identity rotation. Add it alongside rotation support in `ff_window`.

## Device reset (complete)

`ff_dx12_reset_device(bool force)` in `dx12_reset.c`, backed by the
`ff_dx12_device_child` registry in `dx12_device_child.c`.

### Design decisions

- **A tagged intrusive registry, not a vtable.** The legacy `device_child_base`
  was a C++ abstract class. The C port registers an `ff_dx12_device_child` node
  embedded in each owner, carrying a `type` enum and an `owner` pointer, and
  `dx12_reset.c` dispatches with a `switch`. No function pointers, no dynamic
  allocation, and the whole reset sequence is readable in one file.
- **The type enum value *is* the reset priority.** One enum orders both passes:
  teardown walks it in reverse (target_window first, so it drops its back
  buffers before the resource pass sees them) and rebuild walks it forward.
- **Reset only when needed.** Mirrors the legacy `reset_device`. A stale DXGI
  factory (`!ff_dx12_factory_current()`) only rebuilds DXGI; the device is reset
  on top of that only when the adapter hash actually changed, when the device is
  invalid, or when the caller passes `force`. A healthy device is left alone.
- **Allocators survive a reset.** `ff_dx12_mem_range.owner` and
  `ff_dx12_descriptor_range.owner` are raw pointers held by user objects across
  a reset, so destroying the allocators would dangle every outstanding range.
  `destroy_d3d(for_reset=true)` skips `destroy_allocators()`, and each
  allocator's `before_reset`/`reset` swaps only the `ID3D12Heap` /
  `ID3D12DescriptorHeap` inside, preserving every offset and index.
  `internal_ff_dx12_allocators_before_reset`/`_reset` walk the
  `s_*_allocator_valid[]` flags so a lazy accessor never creates an allocator
  against a dying device.
- **No keep-alive during reset.** `before_reset` releases GPU objects
  immediately. Deferring them would keep references that block the device from
  being released, and every fence value naming work on the old device is about
  to become meaningless anyway.
- **One command list for the whole rebuild.** Buffers re-upload their contents,
  so a single `ff_dx12_commands` is opened on the copy queue for the entire
  rebuild walk and executed once after it.
- **Mid-walk add and remove are safe.** `reset_begin` marks every registered
  child `pending_reset`; `walk_next` skips unmarked nodes, so a child created
  during the reset is skipped by all three passes and starts clean.
  `remove_device_child` advances the single `s_walk_cursor` off a dying node
  before unlinking it. A generation counter does not work here because there are
  multiple passes and the first would consume the stamp.

### Bugs found while building it

- **`wait_for_idle` was skipped on every reset (the important one).**
  `destroy_d3d` keyed the drain off `!for_reset`, on the assumption that a reset
  means a dead device whose queues can never signal. That only holds when the
  device is *actually lost*. On a forced or adapter-change reset the GPU is
  still executing, and since `before_reset` releases immediately, resources were
  being pulled out from under running work. Symptoms were wildly varied and all
  downstream: descriptor free-list asserts, a residency list holding stack
  addresses from a previous test, access violations in `reset_begin`. The
  correct predicate is `ff_dx12_device_valid()`, applied both in `destroy_d3d`
  and at the top of the reset body.
- **`s_reset_count` leaked across `ff_dx12_destroy`.** A file-static counter in
  a test host that runs many tests in one process made every reset-count
  assertion after the first fail. `internal_ff_dx12_reset_shutdown()` now clears
  it and is called first from `ff_dx12_destroy`.

### Verifying the tests are not vacuous

Two fault injections were run against `dx12_reset_tests.cpp`. Stubbing
`child_reset` to return `true` without doing anything failed the committed-
resource and texture-view tests and crashed the host on the third. Flipping
`destroy_d3d`'s allocator guard to destroy allocators during a reset crashed the
host immediately, confirming the "allocators must survive" invariant is
genuinely load-bearing and genuinely covered.

One test assumption had to be corrected: comparing `ID3D12Resource*` before and
after a reset is not a valid "it was rebuilt" check, because the allocator
legitimately reuses the freed address. `ff_dx12_resource_reset_count()` is the
reliable signal, and that intermittent-failure-only-in-the-full-suite behaviour
was the test's fault, not the library's.

### Still open

- Whether the queues can be destroyed and lazily recreated across a reset is
  unverified: `ff_dx12_fence.owner_queue` and `ff_dx12_fence_value.fence` are
  both raw pointers.
- `ff.test.c` exits on device loss rather than calling `ff_dx12_reset_device`.

## Reset review pass: cross-object resets

The first reset implementation was reviewed again specifically for what happens
when many objects reset at once, rather than one at a time. Eight cross-object
tests were added first (placed-resource ranges, descriptor ranges, a borrowed
`target_texture`, static-buffer re-upload, all six child kinds together, GPU
pinned and ring descriptors, and back-to-back resets with a destroy in between).
Those all passed, which narrowed the search to state that *outlives* a reset.

### Bugs found and fixed

- **A mapped buffer across a reset corrupted the ring allocator (the bad one).**
  `ff_dx12_buffer_map` parks a ring `mem_range` in `mapped_range`, but
  `internal_ff_dx12_mem_allocator_before_reset` zeroes the ring's
  `allocated_range_count`. Destroying the buffer afterward then freed a range
  the ring no longer knew about, tripping `FF_ASSERT(allocated_range_count > 0)`
  and underflowing the counter to `SIZE_MAX`. The repro killed the test host
  outright. Worse, the mapped CPU pointer was verified to be byte-identical
  after the reset even though `internal_ff_dx12_heap_before_reset` had already
  `Unmap`ped and released the resource behind it, so a write through it landed
  in freed memory and `unmap` would submit a copy from it. Fixed by adding
  `internal_ff_dx12_buffer_before_reset`, which drops `mapped_range` without
  freeing it (freeing is what underflows), and by relaxing `unmap` to a
  `FF_CHECK_RET` so a map interrupted by a reset is a no-op rather than an
  assert.
- **The reset arena grew on every reset.** `internal_ff_dx12_resource_reset`
  re-initializes `global_state` into `resource->arena`, abandoning the previous
  spilled overflow block. For a resource whose subresource states have diverged
  past `FF_DX12_RESOURCE_STATE_INLINE_MAX`, that leaks a block per reset.
  `before_reset` now calls `ff_arena_reset` once every arena consumer is torn
  down, then re-seeds `global_reads`.
- **`internal_ff_dx12_buffer_reset` asserted on a NULL command list.** It took
  `FF_ASSERT_RET_VAL(buffer && commands, false)` at the top, but `dx12_reset.c`
  passes NULL whenever `ff_dx12_queue_new_commands` fails. That turned one
  failure into a failed assert for every buffer in the walk. The `commands`
  check moved below the `kind != gpu_static` early-out, since only a static
  buffer re-uploads.
- **Texture, depth, and `target_texture` resets swallowed failures.** All three
  returned `void`, so a failed view creation never reached `dx12_reset.c`'s
  `result` and the reset still logged "all children rebuilt". They now return
  `bool`.

### Fault injection

Both of the memory bugs have a regression test that was confirmed to fail
without its fix: removing the `mapped_range` clear fails
`destroying_a_mapped_buffer_after_a_reset`, and removing the `ff_arena_reset`
fails `repeated_resets_do_not_grow_a_resource_arena`. The arena test needed a
forced state divergence to be non-vacuous; the first version passed either way
because nothing had spilled out of inline storage.

## Reset review pass: object cache and reset gating

A third review pass covered `dx12_resource.c` line by line and diffed the C
reset path against the legacy `ff::dx12::reset_device`. Three fixes came out of
it.

**`global_state` dangled after the arena rewind.**
`internal_ff_dx12_resource_before_reset` rewinds the resource arena and was
re-seeding only `global_reads`. `ff_dx12_resource_state` also allocates from
that arena for its per-subresource overflow, so the rewind left
`global_state.overflow` pointing at reclaimed memory, readable by anything that
touched the resource between `before_reset` and the reset pass, including a
`destroy` on a resource whose reset failed. Both consumers are now re-seeded
immediately after the rewind.

**A stale DXGI factory forced a full device reset.**
`reset_needed` returned true whenever `IsCurrent()` was false, and that fed the
`force || needed` gate, so any stale factory tore down the device and every GPU
resource. The legacy C++ only sets `force` when the adapter *hash* actually
changed; a stale factory on its own rebuilds DXGI and nothing else. `IsCurrent()`
goes false for benign reasons such as display topology and mode changes, so this
was throwing away the whole device routinely. `reset_needed` now reports only
`!ff_dx12_device_valid()` and `dxgi_stale` is a pure out-param.
`ff_dx12_simulate_factory_stale` was added as a test hook, mirroring the
existing `s_simulate_device_invalid` precedent, and is cleared whenever the
factory is recreated.

**`ff_dx12_object_cache` is now a device child.**
It memoizes `ID3D12RootSignature` and `ID3D12PipelineState`, both device-owned,
in 64-bucket hash tables, and is caller-created and unbounded, which is exactly
the registry's criterion. Because it is a pure memo keyed by a hash of the
caller's desc, it needs no reset pass: `before_reset` releases everything and
empties both bucket arrays, and it refills lazily on the next miss against the
new device. Freed entries are pushed onto `entries_free`, so repeated
empty/refill cycles never grow its arena.

Residency across a reset was re-checked and is correct: every `residency_data`
is unregistered by its owner's `before_reset` (resources directly, heaps through
the mem allocator's buffer walk), and `ff_dx12_residency_destroy` asserts the
pageable list is empty, so a missed unregistration would fail loudly rather than
leave fence values pointing at destroyed queues.

Four tests were added: three in `dx12_object_cache_tests.cpp` and
`a_stale_factory_alone_rebuilds_dxgi_but_not_the_device` in
`dx12_reset_tests.cpp`. All four were fault-injection verified. The first three
object-cache tests were vacuous on the first attempt: they only asserted the
cache still returned a working object, which it does either way since a stale
entry is still a readable pointer. They needed `ff_dx12_object_cache_size` as a
direct observable of "the buckets are empty". This is the second time in this
area that a reset test proved nothing by checking only "it still works".

## Current state and next steps

Milestones 1-5 and device reset are complete. The suite is 915 passing, zero
skipped, across 22 dx12 test files in `test/ff.test.unit.c/dx12/`, stable over
two consecutive full runs.

### The `ff.test.c` sample

`test/ff.test.c/main.c` is now a real end-to-end consumer of the stack: it
creates the main window, initializes DX12 and a swap chain plus a small
CPU-updated texture, and runs a `PeekMessage` loop that clears the back buffer
to black, uploads the texture, and `CopyTextureRegion`s it to a moving position
on the back buffer. No shaders are involved yet, so this works today and will
keep working as a smoke test once the draw device lands.

It is wired to the window through `ff_window`'s `ff_signal`, which is also the
first real use of the deferred-resize pattern that `target_window` itself does
not implement: `WM_SIZE` records a pending size, `WM_ENTERSIZEMOVE` /
`WM_EXITSIZEMOVE` coalesce the flood of sizes from an edge drag into one resize
at the end, and the resize is applied from the loop rather than from inside the
window proc. Graphics are torn down from `WM_DESTROY`, which is the last message
where the `HWND` is still valid and therefore the last point at which the swap
chain can legally be destroyed.

Two bugs were found and fixed in the sample itself:

- **Busy-spin when nothing is rendered.** Presenting is what paces the loop, via
  the frame-latency waitable. Any path that skips presenting (a minimized window
  has a 1x1 client area, and a failed device has no valid target) skipped the
  only blocking call, and the loop then burned a full core. Measured at 4.56 CPU
  seconds per 5 seconds of wall time while minimized. `render_frame` now reports
  whether it presented, and the loop calls `WaitMessage` when it did not;
  re-measured at 0.
- **Resize failure was discarded.** `set_size` failing leaves the swap chain
  with no back buffers and nothing retries it, so the app would idle forever in
  a permanently non-rendering state. The sample now shuts down instead.

The loop also exits rather than idling if the device becomes invalid, since
rebuilding a lost device belongs in the renderer and not in the sample.

Verified by running it: 700 frames across live edge-resizing, a
minimize/restore cycle, and a clean exit with no debug-layer complaints.

Remaining before a renderer can draw real geometry: see "Milestone 6" below.

## Milestone 6: texture views, math types, shaders, draw state

The old renderer is `dxgi/draw_util.cpp` (1118 lines) plus
`dx12/draw_device.cpp` (981 lines). That is too much for one milestone, so it is
split: milestone 6 builds everything the renderer needs to issue a single
textured draw, and milestone 7 adds the batching that makes it fast. The split
point is deliberate, because everything in milestone 6 is independently
testable, while the batching is only meaningful once a draw works end to end.

### 6a. `ff_dx12_texture_view` (complete)

A sub-range SRV: `array_start` / `array_count` / `mip_start` / `mip_count` over
an `ff_dx12_texture`. `ff_dx12_texture` already has a whole-resource SRV, and
`ff_dx12_resource_create_shader_view` already takes the four range arguments, so
this is mostly a device child that owns one descriptor range.

What was built, and how it differs from the old `texture_view.cpp`:

- A count of 0 means "the rest", resolved at init against the texture rather
  than stored as 0, so the struct never holds a value that needs interpreting.
  Init asserts both starts are in range and neither count runs past the end.
- The descriptor is allocated lazily on first `ff_dx12_texture_view_cpu_handle`,
  matching `ff_dx12_texture`, since a view can be created up front and never
  sampled.
- **The reset hook rewrites the SRV in place rather than freeing the range.**
  The old `reset()` freed the descriptor and let the next use reallocate. That
  is correct but pointless churn here: the CPU descriptor allocators survive a
  reset with their buffers and indices intact (the load-bearing invariant that
  `destroy_d3d(for_reset=true)` is built around), so the slot is still valid and
  only its contents are stale. This matches `internal_ff_dx12_texture_reset`.
- The old class held a `shared_ptr` to the texture; in C it is a raw
  `ff_dx12_texture*`, so the texture must outlive every view of it.
- Registered as `ff_dx12_device_child_type_texture_view`, placed immediately
  after `_texture` in the enum so a view is reset after the texture it reads.

Two naming collisions came out of this, both resolved by renaming rather than
by picking a worse type name: `ff_dx12_texture_view` was already a *function* on
texture, so that became `ff_dx12_texture_view_handle` (13 call sites), and the
sub-range accessor is `ff_dx12_texture_view_cpu_handle`.

**The tests were vacuous on the first attempt, again.** All ten passed with the
reset hook stubbed out to `return true`. Two reasons: the raw
`D3D12_CPU_DESCRIPTOR_HANDLE` cannot be compared across a reset at all (the heap
is rebuilt at a new address, so the handle legitimately changes, and the first
version of the test asserted it stayed equal and failed for the wrong reason),
and a descriptor that is never refreshed still reads as a perfectly valid
handle. The fix was `ff_dx12_texture_view_reset_count`, incremented only when
the SRV is actually rewritten, plus asserting on `view.view.start` as the stable
slot identity. With those, stubbing the hook fails 2 of the 10.

10 tests in `dx12_texture_view_tests.cpp`; suite at 894.

### 6a-2. Simple PNG decoding

Done right after `texture_view`, and deliberately not later. Everything it needs
already exists: `ff_dx12_texture_update` and the upload allocator are built and
exercised, `ff_file_map_init` supplies the bytes, and `ff.base.c.vcxproj`
already has a `ProjectReference` to `ff.vendor.libpng.vcxproj` that nothing uses
yet (`data/compression.c` already links zlib the same way). Nothing later in
milestone 6 makes this easier, and doing it first pays off twice: `ff.test.c`
stops uploading a procedural gradient and starts uploading a real image, where a
wrong row pitch or swapped channel is obvious on screen instead of plausible,
and when sprites first draw there is only one new thing being debugged.

**DirectXTex is not an option.** The old `png_image.cpp` returns
`DirectX::ScratchImage`, which drags in a C++ library this project cannot
reference. The C port decodes into an arena buffer instead and hands that
straight to `ff_dx12_texture_update`. Compressed/block formats are the real
reason the old code wanted DirectXTex; that problem is deferred until something
actually needs BCn, and will be solved without DirectXTex.

Scope for the first version, kept deliberately narrow:

- 8-bit RGB and RGBA, non-interlaced, decoded to `R8G8B8A8_UNORM`. libpng's
  transform calls (`png_set_expand`, `png_set_strip_16`, `png_set_gray_to_rgb`,
  `png_set_add_alpha`) normalize most other inputs into that one path cheaply,
  so accepting them is close to free; interlacing is the one case worth
  rejecting outright rather than half-supporting.
- Output goes into a caller-supplied `ff_arena`, following the house pattern:
  the row pointer array libpng needs is a temporary that can come from an
  `ff_arena_init_external` over a stack buffer for typical heights, spilling
  only for very tall images.
- No mip generation, no format conversion beyond the above, no premultiply. The
  caller decides what to do with the pixels.
- **No palette support in this version.** The old reader has a whole second half
  for it (`has_palette`, `palette_`, `trans_palette`, `trans_color`, and a
  separate palette image returned alongside the pixels). Indexed PNGs are the
  input format for the palette renderer, so that path gets pulled in with the
  rest of the palette work rather than guessed at now. Until then an indexed PNG
  is either expanded to RGBA by `png_set_expand` or rejected; expanding is the
  simpler choice and keeps the narrow path honest.

libpng error handling is the one genuinely non-obvious part: it reports errors
by `longjmp` to a `setjmp` the caller installs, so the decode function owns a
`setjmp` and must make sure every arena and libpng struct it created is cleaned
up on that path as well as the normal one.

WIC was considered as an alternative and rejected: it flattens indexed PNGs,
which loses exactly the palette data the renderer will need later, and it adds a
COM boundary. libpng is already referenced and already gives the palette.

### 6b. Math types, pulled in as needed

`ff_color`, `ff_matrix` (4x4), `ff_matrix_stack`, `ff_transform`, `ff_viewport`.
The old `types/` versions total roughly 460 lines and are pure value types, so
they port almost directly once the operator overloads become named functions.
`matrix_stack` is the one with real behaviour: the old `draw_base` exposes
`world_matrix_stack()`, and the renderer pushes and pops around nested draws.

### 6c. Shader delivery

The old path is `object_cache::shader(resource_provider, name)`, backed by a
global resource provider that no longer exists for C code. The HLSL sources are
in `source/ff.application/assets/shaders/`: `vs_sprite`, `vs_line`,
`vs_triangle`, `vs_rectangle`, `vs_circle`, `ps_sprite`, `ps_color`, plus
`data.hlsli` and `functions.hlsli`.

The replacement is compiled `.cso` blobs loaded from disk next to the
executable, looked up by name. This keeps the object cache's existing shape: it
already memoizes root signatures and PSOs, and a shader blob cache is the same
pattern. Loading from disk rather than embedding keeps the build simple now and
can be swapped for embedded blobs later without changing callers.

### 6d. Draw device state layer

Root signature (VS constants 0 and 1, PS constants 0, sampler table, texture
table, palette tables), the PSO permutation matrix, and the three constant
buffer layouts (`vs_constants_0`, `vs_constants_1`, `ps_constants_0`). The old
code keys PSO permutations off a `state_t` flag set that includes
`target_palette`, which selects a different pixel shader.

Also in scope here, pulled in only as the code needs them:

- `format_util` - `color_format`, `palette_format`, `has_alpha`,
  `supports_pre_multiplied_alpha`, `fix_format`. `parse_format` is only needed
  once assets are loaded by name, so it can wait.
- `gpu_event` / PIX markers on `ff_dx12_commands` (`begin_event` / `end_event`).
  Worth doing early in this milestone rather than late, because it is the main
  tool for debugging everything after it.

Deferred to milestone 7 or later: `sprite_data` (as late as possible), the
palette stack (`palette_base`, `palette_data`, `palette_cycle`, pulled in when
the renderer reaches the palette path), `render_targets`, WIC image decoding,
and window-message-driven resize inside `target_window` rather than only in the
sample.

## Review guidance for new subsystems

Three of the last four bugs were in teardown and destroy ordering rather than in
steady-state rendering, so any new subsystem should be checked against:

- **Raw pointers into a resource held by something longer-lived.** Only the
  `ID3D12Resource` is protected by keep-alive. Every other back-pointer must be
  scrubbed on destroy (`resource_tracker_forget`, `residency_set_remove`).
- **CPU waits on fence values that were never submitted.** `signal_later`
  reserves a value that nothing signals until the list executes. Use
  `ff_dx12_fence_value_wait_is_pending` before any CPU block. `_complete` is not
  sufficient: an unsubmitted value is neither complete nor waitable.
- **Teardown order.** After the queues are destroyed, fence values are dangling
  and carry no information; do not read them.
- **Fixed-capacity arrays.** The overflow path must degrade safely (return
  invalid, or grow), never silently drop GPU work or block.
- **Vacuous tests.** Assert that a test actually reaches the state it names; a
  passing test that never exercises its target is worse than no test.

## Frame pacing rework (`ff_dx12_pacing`)

The legacy C++ `target_window::update_pacing` was ported faithfully, bugs and
all. A Release benchmark of `ff.test.c` exposed them: across three 20-second
runs the hitch rate was 0.17%, 2.0% and 60.5%. In the bad run the ladder latched
at a median of 24.6ms for the full 20 seconds and never recovered, with the
reported frame rate oscillating between 56 and 71 fps.

Three defects in the original algorithm:

1. **Stale EMA across a stage change.** `pacing.average` and `pacing.count` were
   never reset when the stage changed, so the new stage was judged on 16 frames
   measured under the old one. With `good_fps` 58 / `bad_fps` 54 and an average
   near 24ms, the demote test kept re-triggering and the climb-down test could
   never pass. This is what made the ladder latch.
2. **Catch-up frames measured as fast frames.** After a missed vblank the
   latency waitable object is already signaled, so the next frame does not block
   and is timed back-to-back with the previous one. That is the tail of one long
   frame, not a fast frame. Feeding it in produced above-refresh frame rates on
   a 60Hz display and fed the oscillation.
3. **Steering on an average.** An average cannot distinguish a steady 16.7ms
   from an alternating 33ms/0.1ms pair that averages the same, which is exactly
   the pattern defect 2 produces.

The logic now lives in `dx12_pacing.{c,h}` as a pure state machine over
`ff_dx12_pacing`, separated from the swap chain so it can be unit tested with
synthetic frame times. `dx12_target_window.c` keeps only the tick measurement,
the catch-up clamp, and `apply_latency`.

What changed:

- **Count late frames, do not average.** A frame is late when it exceeds the
  refresh interval by half. The stage moves on how many frames missed, which an
  average cannot express.
- **Reseed the average on every stage change** and skip half a window of frames
  afterwards, since `SetMaximumFrameLatency` does not take effect until the
  pipeline drains to the new depth.
- **Clamp catch-up frames** to the refresh interval in `update_pacing`.
- **Demote only after two consecutive over-budget windows.** Dropping vsync is
  visible to the player and must require a sustained problem.
- **Exponential promote back-off, capped at 64 windows.** Each premature
  promotion doubles the clean windows required next time, so a machine that
  cannot hold a stage settles instead of flapping forever.
- **Generous late-frame budget of a quarter window.** This was tuned from
  measurement, not guessed. A budget of 1 frame in 16 caused repeated
  demotions against a ~1% background rate of OS scheduling stalls, which
  dropping vsync cannot fix. Trading away vsync for a stall the ladder has no
  influence over is strictly a loss.
- **Refresh rate read from the monitor** via `EnumDisplaySettingsW` instead of a
  hardcoded 60Hz, so the ladder behaves on 120Hz and 144Hz displays. Re-read on
  resize, since the window may have moved to another monitor.
- **Resize keeps the learned stage** and only discards in-flight measurements. A
  device reset still resets the stage fully, because that is a new device.

The stage order is unchanged and deliberate: vsync is given up before latency.
An extra frame of display latency is far more damaging to a fast action game
than tearing, so latency 1 is held as long as possible.

Result on a 60Hz display, Release, 60 seconds: 59.70 fps, median 16.665ms, p99
16.998ms, 0.502% frames over 20ms, 0.023 CPU cores, and zero stage transitions.

### Verifying the tests are not vacuous

All four guards were confirmed to fail the suite when individually disabled:

| Guard disabled | Tests that failed |
| --- | --- |
| Reseeding the average on a stage change | `average_is_not_carried_across_a_stage_change` |
| Exponential promote back-off | `alternating_load_does_not_oscillate_forever`, `promotion_requires_more_evidence_after_a_failed_attempt` |
| Post-change skip frames | `frames_right_after_an_interrupt_are_ignored` |
| Two-window demote hysteresis | `a_single_bad_window_does_not_demote` |

### Benchmark mode in `ff.test.c`

The sample takes an optional "seconds to run" argument and exits with a summary,
so pacing can be measured non-interactively. It reports median/p99/max frame
time, a count of frames over 20ms, and CPU consumption as *cores used* from
`GetProcessTimes`. Percentiles alone are misleading here: one hitch stays in the
512-frame ring for 512 frames and makes a single event look sustained, which is
why the raw long-frame count is reported alongside them.

The cores metric is the one that proves the CPU is genuinely blocking on the
latency handle rather than spinning. It was validated by injecting a 2ms busy
spin per frame: the reading moved from 0.012 to 0.177 cores while the frame rate
stayed at 60. Note that `ff_log_type_debug` is disabled in Release, so the
sample enables it explicitly.
