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
  `ff_window`'s signals is a follow-up. `ff.test.c` does this itself for now,
  and that implementation is the model for the eventual built-in version.
- Display rotation. See "Display rotation and DPI" below for the full design;
  the short version is that rotated displays currently get identity rotation,
  and the types added in milestone 6b carry a rotation field so support can be
  added later without reshaping any API.

## Display rotation and DPI

Neither is supported yet, and neither blocks anything. This section exists so
that the decision is a deliberate deferral with a known implementation path
rather than something rediscovered later.

### Current state

Resize is complete and verified: `ff_dx12_target_window_set_size` waits for
idle, drops every back buffer reference (draining the keep-alive list, since
`ResizeBuffers` fails while any reference survives), resizes, and rebuilds.
Minimize is handled by clamping the zero-sized client area to 1x1, and a
previous failure that left the buffers missing still forces a rebuild rather
than hitting the same-size early-out. This was exercised against the running
sample across four resizes plus minimize, restore, and maximize, with pacing
holding stage 0 throughout.

DPI is handled far enough to be correct: `window.c` responds to `WM_DPICHANGED`
by moving the window to the suggested rect, and since everything renders in
physical pixels, `GetClientRect` already returns post-DPI pixels and the swap
chain is always the right size. What is missing is a `dpi_scale` concept, which
only matters once there is a *logical* coordinate system — the legacy used
`dpi_scale` exclusively for logical/scaled conversions and never for buffer
sizing.

Rotation is entirely absent.

### Why doing rotation ourselves is faster than letting the OS do it

When the display is rotated and the swap chain presents unrotated buffers, the
composition path has to rotate the image on the way to the scanout hardware.
That is an extra full-screen read-modify-write per frame, and it is pure
overhead: the same pixels, moved, for no visual gain. Rotating in the view
matrix instead costs *nothing* — the vertices are already being transformed by
a matrix, so folding a rotation into it is free. The legacy code understood
this, which is why `get_rotate_matrix` exists as a 4x2 table rather than
relying on DXGI alone.

This matters more in full screen, and for a specific reason worth writing down:

> `SetRotation` only works for flip-model swap chains presented in **windowed**
> mode. In full-screen mode it does not fail, but the swap chain must be set to
> `DXGI_MODE_ROTATION_IDENTITY` or `Present` itself fails.

So in true full screen, handling rotation in the view matrix is not an
optimization, it is the only option. Windowed mode is where the OS *can* do it
for you, and where doing it yourself saves the composition pass and lets the
presented buffer stay in the scanout orientation, which is the one most likely
to hit an efficient direct-flip / overlay path instead of falling back to
composition.

One caveat specific to this codebase: full screen here is a **borderless
`WS_POPUP` window** (`default_window_style` in `window.c`), and
`SetFullscreenState` is never called — so DXGI still considers it windowed and
`SetRotation` remains legal. The full-screen restriction above therefore does
not bite today. It becomes real only if exclusive full screen is ever added,
and the performance argument applies either way.

### What full support requires

Split across the window layer and the graphics layer:

1. **A size type carrying rotation and DPI.** The legacy `ff::window_size` is
   just three fields — `logical_pixel_size` (the client rect in pixels),
   `dpi_scale` (dpi / 96), and `rotation` (a `DMDO_*` value) — plus helpers.
   The key helper is `physical_pixel_size()`, which **swaps width and height
   when the rotation is 90 or 270**.
2. **Querying the rotation.** `MonitorFromWindow` -> `GetMonitorInfo` ->
   `EnumDisplaySettingsW(ENUM_CURRENT_SETTINGS)` -> `DEVMODE.dmDisplayOrientation`.
   `dx12_pacing.c` already calls `EnumDisplaySettingsW` on the same `DEVMODE`
   for the refresh rate, so the query is already proven here.
3. **Sizing the swap chain to the physical size,** i.e. swapped for 90/270.
   This is required by `DXGI_SCALING_NONE`, which the port already uses: the
   buffer must match the output exactly.
4. **Calling `SetRotation`** on every size change, with the legacy's
   counter-clockwise mapping: `DMDO_DEFAULT` -> `IDENTITY`, `DMDO_90` ->
   `ROTATE270`, `DMDO_180` -> `ROTATE180`, `DMDO_270` -> `ROTATE90`. Note the
   90/270 inversion — the DXGI value describes how the *buffer* is rotated to
   reach the display, which is the opposite of the display's own orientation.
5. **Folding the rotation into the view matrix,** which is where the efficiency
   actually comes from. The legacy table is four orthographic-to-NDC matrices
   (mapping `[0..w] x [0..h]` to `[-1..1] x [1..-1]`) with the rotation baked
   in, paired with an `ignore_rotation` variant of each that is just the
   unrotated matrix. `ignore_rotation` means "draw as if the display were not
   rotated", and is what an offscreen intermediate that will itself be rotated
   later needs.
6. **Handling `WM_DPICHANGED`** beyond repositioning, once `dpi_scale` exists.

### What milestone 6b does about it now

Nothing functional. The only requirement is that the types added in 6b leave
room, because retrofitting a field is cheap while reshaping every call site is
not:

- The size/viewport type carries `rotation` and `dpi_scale` fields from the
  start, even though `rotation` is always `DMDO_DEFAULT` and `dpi_scale` is
  always 1.0 today.
- Anything computing a view matrix goes through one function that takes the
  size type, so the rotation table has exactly one place to land later.
- That function takes an `ignore_rotation`-style parameter, or is shaped so one
  can be added without touching callers.

Until then, a rotated display renders upright and correct — the OS composition
pass handles it — just with a cost that will be reclaimed when this is done.

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

Milestones 1-5, device reset, PNG decoding (6a-2), the math types (6b), the
format utilities (6c), and the deferred work queue plus thread-ownership asserts
are complete. The suite is 1065 passing, zero skipped, stable across full runs in
**both Debug and Release**, both warning-free.

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

### Multi-mode restructure

The sample is now a mode host rather than a single hard-coded scene, mirroring
what the old `ff.test.console` menu offered. `test_app.c` owns everything that
is not scene-specific — the window, swap chain, device reset, deferred resize,
message pump, stats, and both reports — and a mode is a table of function
pointers in `test_app.h`:

| File | Contents |
| --- | --- |
| `main.c` | Mode table, command line parsing, usage |
| `test_app.c` | Harness: loop, pacing, stats, window, graphics lifetime |
| `test_blit.c` | `blit` mode, the former `main.c` scene |
| `test_draw.c` | `shapes`, `sprites`, `sprite_perf` |

`load`/`unload` are split from `init`/`destroy` because they answer to different
events: assets are read once, while GPU objects are rebuilt on every device
reset. Folding them together would re-read files from disk on every reset.

Mode selection is a command line argument rather than the old `std::cin` menu.
That keeps `ff.test.c <mode> <seconds>` non-interactive, which is what makes it
usable for catching frame pacing regressions from a script; a keyboard menu
needs an input layer that `ff.base.c` does not have yet.

`shapes`, `sprites`, and `sprite_perf` are registered but fail immediately in
`load` with a message naming the two milestones they need. They cannot work
yet: nothing outside the tests calls `ff_dx12_object_cache_shader` or
`ff_dx12_object_cache_pipeline_state`, there is no root signature matching what
the shaders declare, and without a pipeline state `ff_dx12_commands_draw` cannot
be called at all. Failing in `load`, before a device exists, makes that one
clear line instead of a crash deeper in. The old sprite perf test also drove its
count from `VK_SPACE`, so it will take a count argument until there is input.

Verified: `blit` at 59.44 fps and 0.073 cores over 6 s in Debug, and Release
equivalent, so the pacing path is unchanged by the restructure. Help text, an
unknown mode, and a stub mode each exit 1 with the right message. Full solution
builds warning-free in both configurations and the suite is 1079 passing.

Remaining before a renderer can draw real geometry: see "Milestone 6" below.

## Lifetime audit before milestone 6

A full audit of lifetime management across the dx12 layer, run before adding
more code on top of it. Two real bugs, both in `dx12_queue.c`, plus two
standing-rule risks to resolve before the draw device lands.

**Bug 1: `queue->caches_in_use` is never destroyed — fixed.** `ff_dx12_queue_destroy`
(`dx12_queue.c:369-397`) walked only `queue->caches` and NULLed it. The
second list is pushed at 458-459 and spliced out *only* on a successful execute
at 558-566, so anything left on it at teardown leaked its command lists,
allocators, fence, and resource tracker — all of which hold COM references on
the device, so the device could not be fully released either. Not hit internally
today, since the one in-library caller does execute, but the list was
structurally unreachable from `destroy` on every path. Fixed by walking it with
the same `command_cache_destroy` loop. The caches are arena-allocated and
`command_cache_destroy` does not touch `next`, so the walk is safe.

**Bug 2: fences were signalled for command lists that were never submitted —
fixed.** This is the serious one, because it broke the rule that a GPU resource
is never released while the GPU may still be using it. `ExecuteCommandLists` is
gated on `all_resident` at `dx12_queue.c:586`, but the signal loop ran
unconditionally. `ff_dx12_make_resident` returns false when the GPU is over
budget (`dx12_residency.c:279`), so under memory pressure nothing executed and
the fences retired anyway. A second, rarer path wrote `signal_values` *before*
`ff_dx12_commands_take_cache`, so a NULL cache broke the loop with the value
already recorded.

Three reclamation mechanisms read those fence values as "the GPU is finished":
keep-alive deferred release (`dx12_globals.c:453`), command allocator recycling
(`dx12_queue.c:213`, where resetting an allocator whose work has not retired is
undefined behavior), and the upload ring (`dx12_mem_allocator.c:267`). All three
would have handed back memory the GPU may still be reading.

The fix is not simply to skip the signal. The allocators and ring ranges were
already pushed with that fence value earlier in the same function, so never
signalling would strand them behind a value nothing could ever reach — a leak
instead of a corruption. Instead the value is now signalled through the queue
when work was submitted (ordering it behind that work) and directly on the CPU
when it was not, where it is immediately and truthfully complete. The
`signal_values` write also moved to after a successful `take_cache`.

Worth recording that this is inherited rather than a porting mistake: the C++
original has the same unconditional `fence_values.signal(this)` after an
`if (all_resident)` guard at `ff.application/graphics/dx12/queue.cpp:168-173`.

Two tests were added. `abandoned_commands_are_released_by_queue_destroy` was
**vacuous on its first attempt** — merely surviving teardown passed with the bug
still present, because the leak is a refcount, not a crash. It now holds its own
reference to an abandoned command list and asserts the final `Release` returns 0;
re-injecting the fault makes it fail with `Expected:<0> Actual:<1>`.
`executed_fence_values_are_complete_after_idle` covers the normal path. The
residency-failure path is not covered, since reaching it needs real GPU memory
exhaustion; that gap is deliberate and noted here rather than papered over.

Suite is 1081 passing in both Debug and Release.

**Verified clean**, each traced rather than assumed:

- No path in `ff_dx12_destroy` or device reset releases before waiting.
  `destroy_d3d` waits at `dx12_globals.c:1016-1019`; `ff_dx12_reset_device` waits
  at `dx12_reset.c:141-144`, and skipping the wait for an already-lost device is
  correct because its queues can never signal again.
- Every failing `_init` leaves the object inert. The house pattern is "zero
  first, then call your own `_destroy`". The subtle case is handled:
  `ff_dx12_resource_init_placed` clears `mem_range` before destroying so the
  *caller's* range is not freed (`dx12_resource.c:134`).
- Every `_destroy` is idempotent, ending in a full struct zero.
- The upload ring cannot hand out memory the GPU is still reading: `ring_alloc_bytes`
  checks the front range's fence and returns an empty range rather than blocking
  or reusing (`dx12_mem_allocator.c:262-281`). The C port also adds a `back &&`
  NULL guard at 287 that the C++ lacked.
- Descriptor ranges are not freed while referenced. Ring space is fence-gated;
  the eagerly-freeing path serves only CPU-only descriptors that are copied into
  a shader-visible heap at bind time. That invariant is structural rather than
  enforced, so it is worth remembering as the draw device is ported.
- Swap chain resize waits for idle before releasing back buffers
  (`dx12_target_window.c:277-278`), and it is per-resize rather than per-frame.
- No COM pointer stored without an `AddRef`, none released twice.
  `ff_dx12_resource_init_external` AddRefs at `dx12_resource.c:195`. Root
  signatures and PSOs are handed back borrowed and un-AddRef'd, which is safe
  only because nothing caches them across a reset — another unenforced invariant
  the draw device must respect.

**Two standing-rule risks, both now fixed:**

- ~~`ff_dx12_make_resident` blocks the CPU at `dx12_residency.c:209`~~ — **fixed**,
  see "Two-pass eviction" below.
- ~~`ff_dx12_descriptor_buffer_alloc_ring` blocks at
  `dx12_descriptor_allocator.c:318`~~ — **fixed**, see "Non-blocking descriptor
  ring" below.

### Non-blocking descriptor ring

`ff_dx12_descriptor_buffer_alloc_ring` blocked the CPU when the write cursor
wrapped onto a range the GPU had not finished reading. The C++ does the same at
`descriptor_allocator.cpp:189`. With the draw device calling `alloc_range` three
times per flush (`draw_device.cpp:807/823/839`) against a 7936-descriptor ring,
enough per-frame texture churn laps the ring inside a single frame and the block
becomes a full mid-frame CPU/GPU sync.

The fence values involved come from `ff_dx12_commands_next_fence_value`, which is
a *reserved, unsubmitted* value (`dx12_fence.c:58`). There were therefore two
distinct states reaching the wait:

- the current frame's own reserved value, which nothing will ever signal, so
  waiting could never return. The port already failed this case quietly via
  `wait_is_pending`, which the C++ lacks; that guard was the only thing standing
  between a lapped ring and a permanent hang.
- a previous frame's submitted-but-unfinished value, which is what actually
  blocked.

Both now take the same path: reclaim only ranges that are already complete, and
otherwise return an invalid range so the caller can flush and retry. This is the
same shape the upload ring already used at `dx12_mem_allocator.c:262-281`, so the
two rings no longer disagree about what a full ring means.

`ring_fails_instead_of_blocking_on_submitted_gpu_work` covers this by stalling the
queue behind an unsignalled gate fence, asserting the value is genuinely
incomplete, filling the ring, and requiring the wrapping allocation to fail. It
then releases the gate and requires the same allocation to succeed, so the test
pins both halves of the contract rather than just "returns invalid". Restoring
the blocking wait makes it fail on the first assertion. Run five times to confirm
it is not timing-sensitive.

Callers must now treat an invalid range as "flush and retry", not as an error.

### Two-pass eviction in `ff_dx12_make_resident`

The eviction loop blocked the CPU on `wait_to_evict` every time it evicted
anything, reachable per-frame from `ff_dx12_queue_execute_many` under memory
pressure. The C++ original has the identical unconditional
`wait_to_evict.wait(nullptr)` at `residency.cpp:114`, so this was inherited
rather than a porting regression. The C++ author described the fix in a comment
at `residency.cpp:80-82` but never implemented it.

The wait cannot be moved onto the GPU: `Evict` is a CPU-immediate call and D3D12
requires the GPU to be finished with a pageable before eviction, so a fence wait
ordered into the queue does not help when `Evict` runs right after on the CPU.

`evict_pass` is now called twice. The first pass takes only pageables whose
`keep_resident` values are already complete, so `wait_to_evict` stays empty and
the block costs nothing. Only if that frees too little does the second pass take
in-flight pageables and pay for the wait. This works in practice because the loop
already skips anything used this frame via `usage_counter`, so candidates come
from previous frames and their fences are normally retired.

This narrows the window rather than closing it: a genuinely full budget still
blocks in the fallback pass, which is the right trade against failing the frame.

Testing needed a seam, since the real budget is far too large to ever evict.
`ff_dx12_simulate_video_memory_budget` forces the reported budget, following the
existing `ff_dx12_simulate_factory_stale` hook, and is cleared by
`ff_dx12_update_video_memory_info`.

Two tests, both fault-injected by deleting the second pass. That injection makes
`over_budget_still_evicts_when_work_is_in_flight` fail while
`over_budget_evicts_an_unused_pageable` still passes, which proves the first pass
really does evict without blocking and the second really is required.

Two rounds of correcting the tests themselves:

- The first version of the in-flight test signalled its fence on a real queue,
  which retires almost immediately, so the first pass saw it complete and the
  test passed with the second pass deleted. It now stalls the queue behind an
  unsignalled gate fence released from a helper thread, and asserts
  `ff_dx12_fence_value_complete` is false before calling `make_resident`.
- It also passed `commands_fence` as the commands value, which CPU-signalled the
  same fence the in-flight value came from and retired it early. The commands
  value now comes from a separate `submit_fence`.
- Both tests now do all teardown before asserting. Asserting first threw past
  `release_gate.join()`, and `std::thread`'s destructor called `terminate`, so a
  failure crashed the test host instead of reporting which assertion failed.

### The test app harness

Reviewed alongside the above, since it was the newest code. One real bug:
`blit_load` initializes a heap arena before anything that can fail, but
`ff_test_app_run` returned early on a failed `load` without calling `unload`, so
every failed load leaked a Win32 heap. Fixed by applying the same rule the
library follows — a failed init gets destroyed rather than abandoned. Verified by
renaming the asset away and confirming a clean exit. `ff_arena_destroy` is
idempotent (`base/arena.c:288`), so the normal path's second destroy is safe.

A comment claiming the sprite uploads only when the texture is new was also
wrong: the upload is unconditional and runs every frame. The comment was
corrected rather than the behavior, since the per-frame upload is what makes
this a useful ring buffer smoke test.

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
- **Palette support was included after all**, against the original plan for this
  section. The deferral reasoning was that indexed PNGs are the palette
  renderer's input format and that path should be designed with the rest of the
  palette work. In practice the libpng side of it is small — read `PLTE` and
  `tRNS`, and use `png_set_packing` for sub-byte indexes — and skipping it would
  have meant coming back to rewrite the transform setup, since the expand path
  and the keep-indexes path are mutually exclusive choices made before
  `png_read_update_info`. `ff_png_decode` takes a `keep_palette` flag: when it
  is false an indexed PNG is expanded to RGBA exactly as described above, and
  when it is true the raw indexes and a 256-entry RGBA palette are returned
  **in addition to** the expanded pixels, so a caller that does not care about
  palettes never has to know which path ran. What is still deferred is
  everything above the decoder: the separate palette texture, palette remapping,
  and `trans_color` for non-indexed images.

libpng error handling is the one genuinely non-obvious part: it reports errors
by `longjmp` to a `setjmp` the caller installs, so the decode function owns a
`setjmp` and must make sure every arena and libpng struct it created is cleaned
up on that path as well as the normal one.

WIC was considered as an alternative and rejected: it flattens indexed PNGs,
which loses exactly the palette data the renderer will need later, and it adds a
COM boundary. libpng is already referenced and already gives the palette.

**Status: done.** `data/png.{c,h}` with 20 tests in `base/png_tests.cpp`. No
project or include-path changes were needed beyond adding the files —
`build/cpp.targets` already puts `vendor` and `vendor\libpng_inc` on the include
path, so `#include <libpng/png.h>` resolved and the unused libpng
`ProjectReference` linked on the first try.

The tests build their PNG inputs with libpng's *writer* at test time rather than
checking in binary fixtures, so every case is decoding a genuinely well-formed
file. Coverage: RGB, RGBA, gray, gray+alpha, 16-bit strip, indexed both with and
without `keep_palette`, 4-bit packed indexes, `tRNS` palette alpha, agreement
between the index output and the RGBA output, tight row packing, the row array
spilling past the stack arena, interlace rejection, and four malformed-input
cases (non-PNG, empty, truncated, corrupt) that exercise the `longjmp` cleanup,
plus a 500-iteration failure loop that would show a leaked png struct.

Five separate faults were injected one at a time to confirm the tests are not
vacuous — dropping the interlace rejection, the `tRNS` alpha, `png_set_packing`,
`png_set_gray_to_rgb`, and `png_set_strip_16`. Each was caught, and only by the
tests that specifically target it.

One thing worth recording: the first test run crashed the test host with
"libpng error: No IDATs written into file". That was a bug in the *test's*
encoder, not the decoder — writing an interlaced PNG requires looping over the
passes returned by `png_set_interlace_handling`, not a single pass over the
rows.

**`ff.test.c` now loads a real PNG.** The procedural gradient is gone; the
sample maps `assets/sprite.png` (256x256 RGBA) next to the executable via
`ff_file_module_path`, decodes it into a long-lived arena once at startup, and
uploads it to the texture. The texture size now comes from the image rather
than a `SPRITE_SIZE` constant, so the decoded dimensions are load-bearing.

The decoder emits RGBA and the swap chain is `B8G8R8A8_UNORM`, so the sample
swizzles red and blue after decoding rather than introducing a second texture
format. That swizzle is the one piece of this that a screenshot can catch and a
unit test cannot.

Verified by screen capture rather than by eye: the rendered sprite was compared
pixel-for-pixel against the source PNG and matched exactly, **0 of 65536 pixels
differing**. Removing the swizzle as a fault injection made **all 65536**
differ, which confirms the comparison is actually testing something. A wrong row
pitch or a half-uploaded image would fail the same check.

Pacing was unaffected by the switch to a real texture: 30 s at 59.67 fps,
median 16.674 ms, p99 18.470 ms, 0.558% of frames over 20 ms, and the ladder
stayed at stage 0 (one frame of latency, vsync on) with zero transitions.

### 6b. Math types, pulled in as needed — DONE

Implemented. `base/point.h` and `base/rect.h` hold the generally useful geometry
types; `dx12/dx12_color.{h,c}` and `dx12/dx12_matrix.{h,c}` hold the types only
the DX12 renderer will ever use. 81 tests in
`test/ff.test.unit.c/base/math_types_tests.cpp` cover all 73 public functions,
suite now 1016.

Because the tests compile as C++, they use DirectXMath as an independent oracle
for the hand-written matrix code: identity, translation, scaling, multiply,
transpose, and point transform are each asserted against the `XMMatrix*`
equivalent, and the view matrix is checked against the legacy
`translate * scale * rotate_0` composition copied from `draw_util.cpp`. The C
library itself stays DirectXMath-free, since those headers are C++ only.

`test/ff.test.unit.c/main.cpp` installs a module-wide assert listener that fails
any test tripping an assert, which would make the `FF_ASSERT_RET_VAL` guards
unreachable from a test. `scoped_assert_counter` in the test file swaps in a
counting listener for the duration of one test so the guards can be verified
rather than merely trusted.

Six faults were injected and each was caught by exactly the tests written for
it: transposing the multiply result (4 failures), dropping the palette
index-zero transparency rule (exactly the 2 transparency tests, with the other
2 palette tests correctly unaffected), relaxing `ff_rect_float_intersects` to
`<=` (only `touching_rects_do_not_intersect`), removing the `index_remap` NULL
check (3 access violations), removing the remap bounds check (only the
out-of-range test), and making the rotation bounds assert off-by-one (only the
out-of-range rotation test).

`ff_dx12_target_size` carries `rotation` and `dpi_scale` today even though they
are always `ff_dx12_rotation_none` and `1.0`, and `ff_dx12_view_matrix` already
takes `ignore_rotation` and indexes the full 4x2 rotation table. The rotation
work is therefore additive: the table is populated and tested, and nothing
reads a real orientation yet.

The original list here was `ff_color`, `ff_matrix`, `ff_matrix_stack`,
`ff_transform`, `ff_viewport` — copied from the old `types/` folder rather than
derived from what the renderer consumes. Auditing the actual usage cut it down,
and added one type that was missing from the list entirely.

**Required before anything can draw:**

- **`ff_point_float` / `ff_rect_float`.** Not on the original list, and the
  biggest real gap: `ff.base.c` has no geometry types at all (only the
  `ff_value` variants for serialization). Every draw entry point in the legacy
  `draw_base` is expressed in points and rects, so these are unavoidable and
  come first. Plain PODs, a handful of inline helpers.

  Decided: these are **structs with named fields**, not bare `float[2]` /
  `float[4]` arrays. The `ff_value` array members look like precedent but are
  not — they live inside a union and are never passed or returned, and every
  constructor takes scalars. In C an array can't be returned or assigned, and an
  array parameter decays to a pointer so the size is unenforced and a point can
  be passed where a rect is expected. Named fields also settle the real
  `left/top/right/bottom` vs `x/y/width/height` ambiguity, which the legacy code
  resolves as the former. Layout and cost are identical either way.
- **`ff_color`.** Needed per vertex and per sprite instance. The legacy type is
  a tagged union of an RGBA float4 and a `{palette index, alpha}` pair, which is
  how a palette sprite gets its color through the same field as an RGBA one.
  Since palette support is in scope, the union is worth keeping; what can go is
  the operator overloading and the pile of named constant accessors
  (`color_white()` and friends), which become a few `static const` values or
  simple constructors.
- **`ff_matrix` (4x4).** Required by the shader interface, not by convenience:
  `data.hlsli` declares `matrix projection_` in `vertex_shader_constants_0` and
  `matrix model_[128]` in `vertex_shader_constants_1`. Needs little more than a
  4x4 float struct, identity, multiply, and transpose (constants are stored
  transposed for HLSL column-major).
- **A view matrix builder.** One function, per the rotation notes above.

**Deferred, with reasons:**

- **`ff_transform`.** Looks essential but is not, because the sprite path never
  sends a matrix per sprite. `draw_util.cpp:340-348` writes
  position/rotation/scale straight into the instance buffer as `pos_rot` and a
  scaled rect — the GPU does the work. So what milestone 7 needs is the
  *instance layout*, and `ff_transform` is only a convenience struct for
  callers. Worth adding when the draw API is designed, not before.
- **`ff_pixel_transform`.** The fixed-point twin of `ff_transform`, for callers
  that want pixel-snapped positions. It depends on a `ff_fixed_int` type that
  does not exist in `ff.base.c` either. Real value for a 2D game, but purely
  additive: it converts to `ff_transform` at the boundary. Defer until sprites
  draw and pixel snapping is actually wanted.
- **`ff_matrix_stack`.** Its only consumer in the whole legacy engine is
  `animation.cpp` (push/transform/pop around nested animations), which is
  explicitly out of scope. The renderer reaches it through
  `world_matrix_stack()`, but a single current world matrix covers every
  non-animation case; the stack is what nested animations need. The legacy
  version also carries two `ff::signal`s for change notification, used to flush
  batches. Defer the whole thing, and when it arrives, note that the matrix
  *cache* (`world_matrix_to_index`, mapping distinct matrices to slots in
  `model_[128]`) is the part the renderer actually depends on — that belongs
  with milestone 7 batching regardless of whether a stack exists.
- **`ff_viewport`.** Verified unused by any engine code: the only references in
  the entire repo are its own implementation, one unit test, and one perf
  sample. It is letterboxing math (fit an aspect ratio into a target with
  padding) — genuinely useful for a fixed-resolution game, and about 30 lines,
  but nothing needs it to draw. Note that this is *not* the type that carries
  `rotation`/`dpi_scale` from the rotation notes; that is the target size type,
  which is a separate thing.

So the actual 6b scope is: point/rect, color, a 4x4 matrix, and one view matrix
function. That is meaningfully less than the original list, and the deferred
items are additive rather than structural — none of them change the shape of
what gets built first.

`matrix_stack` is the one with real behaviour: the old `draw_base` exposes
`world_matrix_stack()`, and the renderer pushes and pops around nested draws.
Deferred as described above.

**Design constraint carried in from "Display rotation and DPI" above.**
Rotation is not being implemented here, but 6b is where the types that would
have to change are defined, so they are shaped to absorb it later:

- The **target size type** — not `ff_viewport`, which is a different thing and
  is deferred — carries `rotation` and `dpi_scale` fields from the start.
  `rotation` is always `DMDO_DEFAULT` and `dpi_scale` always 1.0 for now, and
  nothing reads them. Adding a field later is cheap; changing the shape of every
  call site is not.
- Every view matrix is built by a single function taking that size type, so the
  rotation table lands in exactly one place when it arrives.
- That function takes an `ignore_rotation` parameter (unused for now) or is
  shaped so one can be added without touching callers. The legacy needed this
  for offscreen intermediates that are rotated later.

The legacy rotation table is four orthographic-to-NDC matrices with the
rotation baked in, so the eventual change is a table lookup in that one
function rather than new work at any call site.

### 6c. Format utilities — DONE

`dx12/dx12_format.{h,c}` replaces the legacy `dxgi/format_util`. The legacy
version answered its questions by calling into **DirectXTex** (`IsCompressed`,
`IsSRGB`, `HasAlpha`). That dependency is deliberately avoided, and it is not
needed: the engine uses a small closed set of DXGI formats, so a static table of
22 entries is both simpler and dependency-free. Adding a format is one table row
rather than a new code path.

Two deliberate differences from the legacy behavior:

- `parse_format` **asserted** on an unrecognized name. `ff_dx12_format_parse`
  returns `DXGI_FORMAT_UNKNOWN` instead, so a caller can report a bad asset
  rather than taking down the process. `ff_dx12_format_fix` maps `UNKNOWN` to
  `R8G8B8A8_UNORM`, so an unparsed name still lands on a usable format.
- The power-of-two test uses `ff_math_is_pow2` rather than the legacy
  `nearest_power_of_two(x) != x`.

This also added `ff_string_equal` / `ff_wstring_equal` to `base/string.h`, which
did not exist (nor did any `memcmp`/`strncmp` use anywhere in `ff.base.c`).
Length is compared before bytes, so a name is never matched by a prefix — which
matters here, since `pal` is both a real name and a prefix of `palette`.

Tests are in `test/ff.test.unit.c/dx12/format_tests.cpp`. Six injected faults
(the `% 4` check, the `mip_count > 1` bound, the `!compressed` term, a prefix
match, a `==`-vs-`<=` length compare, and a wide byte-count) were each caught by
their specific tests. Suite now 1041, Debug and Release.

### 6d. Shader delivery — done

The old path is `object_cache::shader(resource_provider, name)`, backed by a
global resource provider that no longer exists for C code.

The replacement is compiled `.cso` blobs loaded from disk next to the
executable, looked up by name, and memory mapped into the object cache. This
keeps the object cache's existing shape: it already memoizes root signatures and
PSOs, and a shader blob cache is the same pattern. Loading from disk rather than
embedding keeps the build simple now and can be swapped for embedded blobs later
without changing callers.

What shipped:

- `ff_dx12_shader` has **11** entries, not 7. The unit of compilation is a
  (file, entry point, profile) triple, not a file: `ps_sprite.hlsl` has four
  entry points and `ps_color.hlsl` two. `source/ff.application/assets/ff.dx12.res.json`
  is the authoritative list. The enum name doubles as the `.cso` file name and
  the HLSL entry point name.
- A `CompileShaders` MSBuild target in `ff.base.c.vcxproj` drives `fxc.exe` over
  `<FfShader>` items. The metadata is `<Entry>`/`<Profile>` — `Target` is a
  reserved item metadata name and fails with MSB4118. `Inputs` must include the
  `.hlsli` items or editing a shared header will not rebuild the blobs.
- Output goes to a per-configuration `$(FfShaderOutDir)`, shared rather than
  built per project, and each consuming project copies it to `$(OutDir)shaders`.
- Blobs are looked up relative to `ff_module_instance()`, not the process
  executable. Under a test runner the process is `testhost.exe`, so resolving
  against the process would look in the wrong directory.
- Shader blobs deliberately **survive a device reset**. They are device
  independent file bytes, so `internal_ff_dx12_object_cache_before_reset` leaves
  them alone; dropping them would only force a re-map and would invalidate any
  `D3D12_SHADER_BYTECODE` a caller still held. The legacy code made the same
  split, clearing shaders in `on_rebuild_resources` rather than `before_reset`.

Found while testing: package 1.619.6 changed its default copy location to
`.\D3D12\`, but the exported `D3D12SDKPath` still said `.\`, so every
`D3D12CreateDevice` failed. `Microsoft_Direct3D_D3D12_D3D12SDKPath` is now
pinned in `cpp.props` next to the literal it has to agree with.

Seven tests, each fault injected: clearing shaders on reset, swapping a vertex
blob for a pixel blob, and hiding a `.cso` were all caught. The stage check uses
`D3DReflect`, which is the only thing here that would catch a wrong `/T`
profile. Suite now 1072, Debug and Release.

**Memory lifetime review.** Blobs are handed out as raw pointers into a mapping
the cache owns, so the review focused on anything that could move or free that
mapping while a caller still holds bytecode. No defects were found. What makes
it safe:

- The blob lives in a `MapViewOfFile` view, not in the cache's arena. The arena
  relocates its blocks as it grows, so a blob allocated there would move
  underneath a caller.
- `ff_file_map` structs sit in a fixed-size array indexed by the enum, so
  mapping a new shader cannot disturb an existing one.
- PSO hashing hashes shader *contents* (`hash_shader` → `hash_bytes`), not the
  pointer, so two caches with different addresses for the same blob still agree.
- Files open `FILE_SHARE_READ`, so independent caches map the same `.cso`
  without interfering, and destroying one leaves the other's view intact.
- `ff_file_map_destroy` zeroes the struct, making the double-destroy path and
  `destroy` → `init` reuse safe.

Seven more tests cover exactly these: blob stability across 256 unrelated cache
insertions, full byte-for-byte comparison of all 11 blobs after every load, two
independent caches, destroy-then-reinit, double destroy, a handle-count check
across 8 load/destroy cycles, and the out-of-range guard (using a local
`scoped_shader_assert_counter`, since the module listener otherwise fails any
test that trips an assert).

Fault injected three more ways: leaking the mappings in `destroy` was caught by
3 tests including the handle count, writing every blob to slot 0 was caught by 9,
and an off-by-one `BytecodeLength` was caught only by the reflection test —
which is a good argument for keeping it. Suite now 1079, Debug and Release.

One non-issue worth recording, since it looks like a bug on first read:
`ff_file_module_path` and `ff_wide_to_utf8` do not check their arena
allocations, and the loader passes a 1024-byte stack arena. That is safe because
`ff_arena_declare_stack` passes `grow_buffer_size == 0`, which
`ff_arena_init_external` reads as "default from the external size" — the arena
spills to the heap rather than returning NULL. Verified directly with a 64-byte
arena, which still returned the full module directory.

### 6e. Draw device state layer

Root signature (VS constants 0 and 1, PS constants 0, sampler table, texture
table, palette tables), the PSO permutation matrix, and the three constant
buffer layouts (`vs_constants_0`, `vs_constants_1`, `ps_constants_0`). The old
code keys PSO permutations off a `state_t` flag set that includes
`target_palette`, which selects a different pixel shader.

Also in scope here, pulled in only as the code needs them:

- `gpu_event` / PIX markers on `ff_dx12_commands` (`begin_event` / `end_event`).
  Worth doing early in this milestone rather than late, because it is the main
  tool for debugging everything after it.

Deferred to milestone 7 or later: `sprite_data` (as late as possible), the
palette stack (`palette_base`, `palette_data`, `palette_cycle`, pulled in when
the renderer reaches the palette path), `render_targets`, WIC image decoding,
and window-message-driven resize inside `target_window` rather than only in the
sample.

## Running the game on its own thread

The intended end state matches the old C++ design: the game loop and all
rendering run on a dedicated game thread, while the Win32 message loop stays on
the main thread and forwards what the game needs.

**This is very achievable. The DX12 code does not depend on the main thread.**
The reason is structural rather than lucky: DX12 itself has almost no thread
affinity (unlike D3D9/D3D11 immediate contexts), and the one place the OS does
impose affinity — the `HWND` — is already isolated to `ff_dx12_target_window`.

What is already in place:

- **The deferred work queue and thread-ownership asserts are done** (see the
  section below). That was the part genuinely painful to retrofit, so it landed
  before the draw device rather than after.
- `windows/dispatch.{h,c}` is a complete cross-thread dispatcher with the
  `main` / `game` types already defined, a message-only window per dispatcher,
  `post` / `send` / `flush`, and a thread-affinity check. It is the direct
  analogue of the old `ff::thread_dispatch` and needs no new work.
- `ff_dx12_target_window` is the *only* DX12 file that touches an `HWND`, and it
  touches it through `GetClientRect`, `IsWindow`, `CreateSwapChainForHwnd`, and
  `MakeWindowAssociation` — none of which require the window's owning thread.
- The sample already defers `WM_SIZE` into a `size_pending` flag applied at the
  top of the loop rather than resizing inside the message handler, which is
  exactly the shape the threaded version needs.
- `MakeWindowAssociation(DXGI_MWA_NO_WINDOW_CHANGES)` is load-bearing here: DXGI
  does not install its own message hook, so it never needs to interact with the
  message thread.

What has to be added before the loop can actually move:

1. **The game thread state machine.** Port `stopped` / `running` / `pausing` /
   `paused` with the main thread requesting transitions via
   `ff_dispatch_post` to the game dispatcher and blocking on an event until the
   game thread acknowledges. The old code's 5-second timeout with
   `TerminateProcess` (INFINITE under a debugger) is worth keeping: a hung game
   thread is otherwise an unkillable process.
2. **A frame scope that blocks dispatch.** The old
   `allow_dispatch_during_wait` refused to run game-thread work during a frame
   update, precisely so a device reset or resize could not land mid-frame. Any
   wait the game thread performs inside a frame must not pump the dispatcher.
   `ff_dx12_flush_deferred` is already the single place those land, so this is
   really just a rule about where the flush is called from.
3. **`ff_dx12_set_owner_thread` on the game thread.** One call as the thread
   starts, after which the existing asserts enforce the new owner.

The one genuine hazard: `ff_dx12_target_window_destroy` and the swap chain must
be torn down before the `HWND` dies, and `WM_DESTROY` arrives on the *main*
thread. That has to become a blocking `ff_dispatch_send` to the game thread from
the `WM_DESTROY` handler, so the window outlives the swap chain. The sample
already destroys graphics on `WM_DESTROY`; threaded, that call must be a `send`
and not a `post`.

Tracked as the `m7-game-thread` todo, sequenced after the draw device so the
loop being moved is the final one.

## Deferred work queue and thread ownership (complete)

The groundwork for the game thread, built ahead of the draw device because it is
the piece that is painful to retrofit. All of it is correct and tested
single-threaded, so nothing here waits on the game thread actually existing.

**Ownership.** `ff_dx12_init` records its calling thread as the owner;
`ff_dx12_set_owner_thread` hands ownership over (the game thread will call it
once at startup); `ff_dx12_on_owner_thread` reports it. Before init records an
owner, every thread counts as the owner, which keeps the asserts quiet for work
that legitimately runs before the device exists.

`FF_DX12_ASSERT_OWNER()` compiles out in release like any other assert. It
guards `ff_dx12_destroy`, `frame_started`, `frame_complete`, `wait_for_idle`,
`flush_deferred`, and `ff_dx12_target_window_set_size`. This is what lets the
dx12 layer keep having **no locks on any device object** — the absence of
synchronization is now a stated, checked invariant rather than an accident.

**The queue.** `ff_dx12_defer_resize_target` and `ff_dx12_defer_reset_device`
are safe from any thread; `ff_dx12_flush_deferred` applies them on the owning
thread between frames, and `ff_dx12_has_deferred` lets a caller skip the flush
in the common case. A single critical section guards the queue and nothing
else, so it is never held across a D3D call.

Three details that matter:

- **Resizes coalesce per target.** A window drag produces a flood of `WM_SIZE`,
  and each real resize waits for idle, so only the final size is applied.
- **Reset is applied before resizes.** A reset rebuilds every swap chain at its
  *current* size, so resizing first would be thrown away.
- **Destroy cancels.** `ff_dx12_target_window_destroy` cancels any queued resize
  for itself, so the queue can never hold a pointer to a dead target.

`ff_dx12_destroy` drops the queue rather than applying it, since nothing may be
queued against objects that no longer exist.

The sample now queues from `WM_SIZE` and flushes at the top of its loop instead
of resizing inline. Verified still running at **60.0 fps, stage 0, latency 1,
0.083 cores, "blocking (good)"** — one-frame latency and pacing are unaffected.

Tests are in `test/ff.test.unit.c/dx12/dx12_defer_tests.cpp` (13 tests against a
real swap chain, including queuing from another thread and four threads hammering
the queue at once). Three injected faults were each caught by exactly their own
test: removing the coalescing, removing the cancel-on-destroy, and swapping the
reset/resize order. Suite 1041 → 1054, Debug and Release.

An adversarial re-read of the queue afterwards added six edge-case tests (1054 →
**1060**): a resize that fails against a destroyed HWND, minimize clamping 0x0 to
1x1 and restoring, cancelling a target that was never queued and cancelling
twice, cancelling a middle entry (the swap-with-last removal is the case that can
lose a neighbour), `force` surviving a later unforced request, and an unforced
reset of a healthy device being a no-op. Three more injected faults were each
caught: downgrading `force`, dropping the swap-in on cancel, and removing the
zero-size clamp.

Two real defects came out of that read and are fixed:

- **`flush_deferred` looped `while (true)`** with a comment claiming it was
  bounded because "only a real failure re-queues work". Nothing enforced that,
  and the re-queue path the comment described does not actually exist —
  `ff_dx12_device_fatal_error` only marks the device invalid, it never queues a
  reset. The loop is now capped at 8 passes, and the comment says what the code
  really guarantees.
- **The `MAX_DEFERRED_TARGETS` overflow path silently drops the resize when the
  caller is not the owner thread.** `FF_DEBUG_FAIL` fires in Debug but Release
  loses it. Only reachable with more than 8 windows; now documented at the site
  rather than looking like an oversight.

Known and deliberately left for `m7-game-thread`, since neither is reachable
while one thread owns everything:

- **`s_defer_mutex_valid` is read outside the lock**, then `ff_dx12_destroy`
  clears it and calls `DeleteCriticalSection`. A window thread calling
  `defer_resize_target` concurrently with destroy could pass the check and then
  enter a deleted critical section. `dispatch.c` has the same check-then-lock
  shape deliberately, but it never deletes its critical section while another
  thread can still arrive. Fixing this properly means a shutdown handshake, which
  belongs with the game-thread state machine.
- **Cancel reorders the queue** (swap-with-last). Harmless today because targets
  are independent, but it means the queue is a set, not a FIFO — worth
  remembering before anything grows an ordering assumption.

## GPU event markers (complete)

`dx12_gpu_event.{h,c}` plus `ff_dx12_commands_begin_event` / `end_event` /
`set_marker`. Named scopes show up as a nested timeline in PIX and RenderDoc,
which is the main debugging tool for the draw work that follows.

**No WinPixEventRuntime dependency.** `pix3.h` starts with an `#error` for any
non-C++ translation unit, so it cannot be used from `ff.base.c` at all. The
runtime's exports (`PIXBeginEventOnCommandList` and friends) are undecorated and
would be callable from C, but they are not declared in any C-consumable header
and the modern PIX3 blob layout is an undocumented implementation detail. Instead
this uses PIX's *legacy* marker format: `BeginEvent(0, wide_string, byte_count)`,
where a metadata value of 0 means "the blob is a null-terminated wide string."
That format is stable, documented by the `WINPIX_EVENT_UNICODE_VERSION` define,
understood by both PIX and RenderDoc, and needs nothing but `d3d12.h`.

Compiled out when `PROFILE_APP` is 0 (Release), matching the old
`ff::constants::profile_build` guard — markers cost command list space and CPU
time for something no debugger is attached to read.

The old `gpu_event_color` was dropped. The legacy blob format carries no color
field, so it would have been an unused function.

Tests are in `test/ff.test.unit.c/dx12/dx12_gpu_event_tests.cpp` (5 tests). The
name table is `static_assert`ed to cover every enum value and tested for
completeness and uniqueness, since it is indexed directly by enum value.

**What these tests cannot do:** injecting a deliberately wrong blob size (name
length + 4096) did *not* fail anything — D3D12 accepts marker blobs without
validating them, so a malformed blob is silently passed through and only shows up
as garbage inside a capture. The execute-cleanly tests therefore cover the call
plumbing, not the blob contents; the comments in that file say so. Verifying the
blob layout itself needs an actual PIX capture. The name-table faults (a
duplicated name) *were* caught.

The sample wraps its frame in `render_frame` / `draw_2d` markers, so the path is
exercised for real rather than only in tests. Re-measured at **60.12 fps, stage
0, latency 1, 0 long frames**.

## NuGet packages: Agility SDK and PIX (complete)

`source/ff.base.c/packages.ff.base.c.config` references
`Microsoft.Direct3D.D3D12` (1.619.6) and `WinPixEventRuntime` (1.0.240308001,
the newest published), with the versions coming from `build/base.props`
(`D3D12AgilityVersion`, `WinPixVersion`). The project imports both `.targets`
files and has the usual `EnsureNuGetPackageBuildImports` check so a missing
restore fails with a clear message instead of a link error.

Three files hold the version and must change together: `build/base.props` (both
`D3D12AgilitySDK` and `D3D12AgilityVersion`) and the two `packages.*.config`
files for `ff.base.c` and `ff.application`. `D3D12AgilitySDK` becomes the
`D3D12_AGILITY_SDK_VERSION_EXPORT` define, so it has to match
`D3D12_SDK_VERSION` in the package's `d3d12.h` or the runtime rejects the
redistributable and silently falls back to the OS copy.

There is no solution file, so a `packages.config` restore needs
`/t:Restore /p:RestorePackagesConfig=true`, and the repo-root `nuget.config`
sets `repositoryPath` to `packages` — without it, restore drops packages next to
the project instead of where `PackagesRoot` expects them.

**The Agility SDK does nothing without two exported symbols.** `dx12_agility.c`
exports `D3D12SDKVersion` and `D3D12SDKPath`; the D3D12 runtime looks for them in
the *executable* to decide whether to load the redistributable `D3D12Core.dll`
instead of the one in system32. The old C++ code had these in `dx12_globals.cpp`
and the C port had lost them, so the package was being restored and copied while
the process quietly used the OS D3D12. Verified by checking the loaded module
path: it now resolves to the exe directory, and removing the `__declspec` puts it
straight back to `C:\windows\SYSTEM32\D3D12Core.dll`.

Because a static library only contributes object files something references,
those exports live in their own translation unit with an
`ff_dx12_agility_sdk_version()` accessor, which `ff_dx12_init` calls when it logs
the version. That log line is what guarantees the linker keeps the object, and it
also makes the active SDK visible in any capture of the log.

`D3D12SDKPath` is `".\\"`, not the old `".\\D3D12\\"`, because the current NuGet
targets copy `D3D12Core.dll` flat next to the exe.

**The PIX package is referenced for the DLL, not for linking.** Markers use the
legacy blob format recorded directly on the command list, so nothing links
`WinPixEventRuntime.lib` — confirmed with `dumpbin /imports`, which shows no PIX
import in either configuration. The DLL is deployed next to the exe because
PIX *timing* captures look for it there.

**Release does not reference the PIX package at all.** `WinPixEventRuntime.dll`
is not shipped, so `ff.base.c.vcxproj` imports the package's targets only when
the configuration is not Release, and `EnsureNuGetPackageBuildImports` only
demands the package there too. The import has to be gated rather than just
suppressing the copy: the package's targets add `WinPixEventRuntime.lib` to
`AdditionalDependencies` with no configuration condition, so importing it in
Release would put a link dependency on a DLL that is not being shipped. Release
also has no markers to begin with (`PROFILE_APP=0`).

The `$(WinPixRoot)bin\x64\` entry that used to be in `cpp.targets` was removed:
the package's own targets already add that directory for the projects that
import it, so the global copy only served to leak the path into projects that
do not use PIX. `ff.application` (the old C++ code) genuinely calls
`PIXBeginEvent` and includes `pix3.h`, so its import stays unconditional.

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

## Draw state layer (`ff_dx12_draw_state`, complete)

The state half of the draw device: one root signature, two samplers, and a
lazily built matrix of pipeline states. Splitting it out from the batching half
means the batcher can be built on a state layer that already has tests.

A permutation is keyed by `(bucket, blend, depth, target format)`. Blend and
target are multi-bit fields rather than independent flags because their values
are mutually exclusive, and the whole key stays a small integer so the
permutations can index a flat array instead of needing a hash.

`make_flags` takes the target format, not just a `transparent` flag, because
blending has to be forced off for the `R8_UINT` palette target: blending two
palette indexes produces a third, unrelated index rather than a blended color.
The target bit is still set in that case, since it also selects the palette-out
pixel shader.

Depth comparison is `GREATER`, not `LESS`. Depth values increase front-to-back,
so the newest instance at a pixel wins. This matches the old C++.

The object cache is owned by the draw state rather than reached through a
global as it was in C++. Every pipeline and the root signature are borrowed
pointers into that cache, so containing the cache makes the borrow strictly
shorter than the lifetime of what it points at, and two draw states cannot
disturb each other. `destroy` drops the borrowed pointers before destroying the
cache so nothing can observe them dangling.

Samplers come from a pinned range, not the ring: the sampler table is bound for
every draw of every frame, and a ring range would be reclaimed out from under
it. Across a device reset the pinned range keeps its heap slots (so outstanding
handles stay valid) but the descriptors written into them do not survive, so
reset re-creates the samplers without re-allocating the range.

Device-child ordering is load-bearing. `draw_state` sits last in the enum, so
`before_reset` runs on it *first* (dropping borrowed pointers before the object
cache releases them) and `reset` runs on it *last* (after the cache is ready to
refill). This depends on `object_cache` preceding it in the enum.

The `CD3DX12_*` helpers the C++ used for the default blend, rasterizer and
depth-stencil descs are C++-only, so those defaults are written out by hand.
A wrong value there produces a silently different PSO rather than an error,
so they were checked field by field against the D3D12 defaults.

`ff_dx12_draw_state_bucket_ps` exists only for testing, and it was added because
a test was found to be vacuous. The original test compared pipeline pointers for
the color and palette targets and asserted they differed, but those two
permutations also differ by render target format, so they would have been
distinct objects even if both used the same pixel shader. Fault injection (making
the palette path select the non-palette shader) did not fail that test. The
accessor exposes the shader choice directly, `create_pipeline_state` now routes
through it so the test pins the code that actually runs, and the same fault
injection now fails as it should.

All fourteen tests were fault-injected. Four independent faults (blend not
forced off for the palette target, palette-out shader ignored, `before_reset`
not clearing pipelines, and `apply` ignoring its flags) each produced a failure.

### Draw state review pass

A second pass over the draw state layer, after the tests were already green.

**Reserved permutation keys were accepted (fixed).** The blend and target fields
are each two bits with only three legal values, so the fourth pattern of each is
unreachable from `make_flags` but still *in range*. `apply` only bounds-checked
against `ff_dx12_draw_state_count`, so a hand-built key using a reserved pattern
would have silently aliased onto some other permutation instead of being
rejected. `ff_dx12_draw_state_flags_valid` now rejects both patterns and `apply`
asserts on it. Weakening it back to a bare bounds check fails two tests.

**Pipeline creation does validate against the root signature.** This was worth
establishing rather than assuming, because it determines whether the `apply`
tests prove anything about the binding contract. Shrinking the texture range
from 32 to 1 does not merely fail a test, it takes down the test host: the
runtime rejects the mismatch hard. So the passing `apply` tests really do pin
the root signature against what the shaders in `data.hlsli` declare, which is
the most valuable thing this suite checks.

**The hand-rolled D3D12 defaults were verified field by field** against
`d3dx12_core.h` in the Agility package rather than from memory: `CD3DX12_BLEND_DESC`
(:591), `CD3DX12_RASTERIZER_DESC` (:615) and `CD3DX12_DEPTH_STENCIL_DESC` (:318).
All three match, including the fields left zero. The one deliberate difference is
`CullMode`, which the C++ also overrides to `NONE` right after constructing the
default.

**Destroy has a lifetime contract that is now documented.** Command lists do not
hold references on the pipelines they are given, so releasing the cache while
work is in flight would dangle. This is the same contract the object cache
already has, and in practice the draw device holds one draw state for its whole
lifetime and tears it down after `ff_dx12_wait_for_idle`. Worth stating in the
header rather than leaving implicit.

Confirmed correct and left alone: the object cache hashes descs deeply (input
layouts by semantic string, not by pointer), so passing stack locals for the
root signature ranges is safe; shader-visible heaps are bound once per command
list in `ff_dx12_queue_new_commands`, so the sampler table binding works; and the
device-child enum ordering puts `draw_state` after `object_cache`, which is what
makes the borrowed pointers get dropped before the cache releases them.

## Instance bucket (f_dx12_instance_bucket, complete)

The storage half of the draw device, ported from instance_bucket in draw_util.cpp. Sixteen
type-erased growable arrays: eight opaque buckets that line up one-for-one with the eight
`ff_dx12_draw_bucket` pipeline buckets, then eight transparent mirrors in the same order.

Design notes:

- **Sixteen instance buckets, eight pipeline buckets.** Opaque and transparent are separate
  buckets rather than a flag on the instance, because they are drawn by completely different
  strategies: the opaque half is drawn in bucket order with one instanced call per bucket, while
  the transparent half has to be drawn back to front in caller order. They still share a pipeline,
  since blending is selected by the draw state flags. `ff_dx12_instance_bucket_draw_bucket`
  does the mapping, and two `static_assert`s pin the two enums together so the halves cannot
  drift apart.
- **Arena-backed, not `_aligned_realloc`.** The C++ used `_aligned_realloc`; this uses a
  per-bucket `ff_arena` initialized with `ff_arena_init_heap_local`, which may relocate the
  block, so `add` always re-fetches the pointer from `ff_arena_realloc`.
- **Nothing is allocated until the first instance lands.** Most frames touch only a few of the
  sixteen buckets, so the unused ones cost nothing.
- **`clear` keeps the allocation.** Instances are plain bytes with no destructors, so clearing
  is a rewind of the count. Bucket sizes are stable frame to frame, so a steady-state frame does
  no allocation at all. Growth doubles from `FF_DX12_MIN_INSTANCE_BUCKET_COUNT` (64).
- **`render_start` / `render_count` are snapshotted separately from `count`**, because the
  draw happens after the buckets have been copied into the combined instance buffer and cleared.

11 tests in `dx12_instance_bucket_tests.cpp`. Fault-injected with four distinct faults
(`realloc`->`alloc` losing contents, a wrong opaque/transparent mapping, `clear` freeing the
block, and an off-by-one in `add`); every one was caught by the expected tests. One originally
planned guard test (`add_rejects_uninitialized_bucket`) was found to be **non-discriminating** --
an unrelated arena assert fired either way -- and was replaced with a behavioral test that pins
each returned slot to `data + count * item_size` across a reallocation.

Debug 1111/1111, Release 1111/1111.

## Draw device core (f_dx12_draw_device, complete)

The batching half of the draw device, ported from `draw_device_base` in `draw_util.cpp`. Draw
calls append instances into the sixteen buckets instead of issuing GPU work; a flush turns the
whole accumulation into a handful of instanced, indexed draw calls.

Design notes:

- **No sort for transparency.** The C++ has no sort either, which is easy to miss.
  `nudge_depth` hands out strictly non-decreasing depths, so issue order is already depth order.
  The flush only coalesces runs that share a bucket, a depth and contiguous indices.
- **`push_no_overlap` shares one depth across a run** so the instances merge into one draw call.
  The first call inside the region still advances, so the run cannot collide with what came
  before. `pop_no_overlap` ends the run only at the outermost pop.
- **Only `pre_multiplied_alpha` flushes on push/pop**, because it alone changes the pipeline.
  `no_overlap` and `opaque` only affect how later instances are bucketed.
- **Matrices are interned per flush** and transposed once at intern time rather than per instance.
  A full table flushes to make room, since the table is per-flush.
- **Static geometry is generated, not tabled.** The circle index patterns are regular (a fan for
  filled, a quad per segment for outline), so they are built in a loop and verified against the
  C++ table rather than transcribed. Same for the unit-circle vertex ring.
- **Bucket offsets use a modulo round-up, not `ff_math_round_up`.** Instance strides are 68, 80,
  44, 56 -- none of them powers of two -- and `ff_math_round_up` is bitmask-based, so using it
  would have silently corrupted every bucket offset after the first.

Two real bugs were caught during this milestone rather than shipped:

1. `ff_math_round_up` is power-of-two only. Caught by reading its implementation before trusting
   the name.
2. Uploading the static geometry at init time called `ff_dx12_buffer_update` with a NULL command
   list, which **took down the test host** rather than failing cleanly. Fixed by deferring the
   upload to the first `begin`, where a real command list exists, using the purpose-built
   `ff_dx12_buffer_init_gpu_static`. This is the same "vanished test run" symptom the draw state
   milestone documented; recognize it as a hard runtime rejection, not a flaky test.

The five instance struct sizes are pinned with `static_assert`. One of the sizes was guessed
wrong initially and the assert caught it, which is exactly what they are there for.

20 tests in `dx12_draw_device_tests.cpp`, fault-injected with five distinct faults. Note that
injecting several faults at once **masked one of them** -- `nested_no_overlap` only failed once
its fault was isolated, because a co-injected fault changed the path under test. Inject faults one
at a time when a test unexpectedly survives.

Debug 1131/1131, Release 1131/1131.

Deferred to `m7c`/`m7d`: the public `draw_lines`/`draw_triangles`/`draw_rectangle`/
`draw_circle` entry points, and sprites plus palettes (which need texture and palette index
tables and types that do not exist in the C port yet).

## Draw device: upload-direct constants and instances

Three bugs were found in the m7b flush path by reviewing it against the root signature
rather than trusting the code to be self-consistent:

- `ff_dx12_draw_state_bind` was never called, so no draw ever had a root signature, a
  sampler table, or a primitive topology. The debug layer turns this into a device
  removal, which presents as the test host vanishing mid-run.
- `vs_constants_0` is declared as `D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS`, so the
  buffer that was being uploaded for it every flush was never read by anything.
- `instance_buffer` was created with `ff_dx12_buffer_init_cpu`, which has no GPU
  resource at all, so `ff_dx12_buffer_gpu_address` returned 0 and every draw was
  bound to a null vertex buffer.

### Constants and instance data now live in upload memory

Both the per-flush model matrices and the packed instance data are written straight
into a ring allocation from `ff_dx12_upload_allocator()` and bound from there: the
matrices as a root CBV address, the instances as a vertex buffer view. Previously each
went through a default-heap buffer, costing a CPU copy into upload memory plus a GPU
`CopyBufferRegion`, for data the GPU reads exactly once in the draws issued immediately
afterward.

Two constraints make this safe, and both are load-bearing:

- The range is allocated against `ff_dx12_commands_next_fence_value`, so the ring
  cannot recycle it until the GPU has retired the submission that reads it. These
  ranges must never be freed explicitly; `ff_dx12_mem_buffer_destroy` documents the
  same rule.
- Binding from upload memory bypasses `ff_dx12_commands_update_buffer`, which is where
  the copy path used to register the heap for residency. Both sites now call
  `ff_dx12_commands_keep_resident` directly.

A root CBV address must be 256-byte aligned; the upload ring already aligns to
`D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT`.

### Depth is wired through begin

`apply_bucket` previously hardcoded `has_depth = false`, so the depth-enabled pipeline
variants could never be selected and the per-instance depth slices did nothing on the
GPU. `ff_dx12_draw_device_begin` now takes an `ff_dx12_depth*`, and the device tracks
it for the duration of the drawing state.

### Residency never stalls the CPU

`ff_dx12_make_resident` had a second eviction pass that would evict pageables still in
flight and block until the GPU retired them. That runs mid-frame on the submit path, so
it could cost a full frame. Only the non-blocking pass remains: pageables the GPU is
still using stay resident and the allocation goes over budget instead, which the driver
absorbs by demoting pages itself.

### Testing

The batching tests never left the valid state, so the entire flush path was unexercised.
`flush_issues_draws_the_gpu_accepts` drives a real begin/draw/flush/execute against a
render target with two different-stride buckets, so the debug layer validates the root
arguments, pipeline state and vertex views, and the instance address is asserted
directly. A null vertex buffer view is legal in D3D12 and silently draws nothing, so
device removal alone cannot catch that case.

## Residency: eviction must respect the pending make-resident fence

`ff_dx12_make_resident` guards each pageable with two independent fence values:
`resident_value` (the async `EnqueueMakeResident` completion) and `keep_resident`
(the GPU work that references it). `evict_pass` checked only `resident`
and `keep_resident`, so a pageable could be handed to `ID3D12Device6_Evict` while
its enqueued make-resident had not signaled. Evict has no defined ordering
against an in-flight EnqueueMakeResident, so the two residency operations raced.

`keep_resident` usually hid this: if the command fence completed, the work ran,
so residency must already have finished. But `keep_resident` is empty whenever
the caller had no command fence value -- `ff_dx12_fence_values_add` drops a zero
value -- and an empty set reports `true` from both `wait_is_pending` and
`complete`, so it offers no protection at all in that case. That is reachable
from `dx12_queue.c`, where `next_fence_value` stays zero if the cache loop breaks
on its first iteration.

The fix is a `ff_dx12_fence_value_complete(data->resident_value)` check next to
the existing `resident` check. It is free in the common case, since that function
returns true for a zeroed value.

`over_budget_does_not_evict_a_pending_make_resident` covers it: a resident
pageable with an empty `keep_resident` and a deliberately pending
`resident_value` must survive an over-budget pass. The test asserts the empty
`keep_resident` and the pending value up front so it cannot pass for the wrong
reason, and removing the guard makes it fail.

Two related cleanups went with it. `wait_to_evict` was dead: it was filled from
`data->keep_resident` only after `ff_dx12_fence_values_complete` returned true,
and that function *clears* the list when it does, so `add_all` always copied from
an empty set. The whole mechanism was removed along with a comment that credited
the no-op to the wrong cause. And `s_usage_counter` now skips zero on wrap, since
zero is what fresh residency data starts at -- on wrap every never-used pageable
would have looked touched-this-call and stalled eviction at the first one.

## Resource tracker: forget does targeted slot surgery

`ff_dx12_resource_tracker_forget` used to call `index_map_rebuild` unconditionally, rehashing every
tracked resource on each resource destroy. It now removes the resource's slot directly and repairs
the probe chain behind it, then repoints the swap-removed last entry with `index_map_move`. Cost
goes from O(tracked) to O(probe run).

The probe-chain repair is the subtle part. Clearing a slot in an open-addressed map strands any
following entry that probed past it, so the run after the cleared slot is reinserted. The unmap has
to happen *before* the swap-remove, while every slot still resolves against the entry it currently
points at.

Covered by `forget_keeps_the_remaining_resources_findable`, which jitters resource addresses with
odd-sized padding allocations. This matters: resources in a plain stack array have a fixed stride
that the multiplicative pointer hash turns into a collision-free permutation, so an array-based test
never builds a probe run and passes even with the repair loop removed.

`repeat_transitions_stay_on_the_all_same_fast_path` pins the property the sprite renderer's
efficiency rests on: repeat transitions to the current state collapse to the `all_same` early-out
with no barrier, no per-subresource expansion, and no arena spill.

## Barrier promotion/decay: the `global` entry type is permanently `global`

`merge_entry` in `dx12_resource_state.c` only copies the *state value* (not the type) when the
destination entry's type is `global`. Since `ff_dx12_resource_state_init` seeds a resource's
`global_state` with type `global`, that type sticks for the life of the resource and never becomes
`decayed`.

This is load-bearing. `allow_promotion` requires `type_before == global && state_before == COMMON`.
If a decay merge overwrote the type with `decayed`, promotion would fire exactly once per resource
and every later frame would pay a redundant `COMMON -> read` barrier. The `decayed` type is written
into the tracker entry during `tracker_close` purely to satisfy `assert_type_change` bookkeeping
within that close pass, and is discarded at the merge on the next line.

Promotion/decay coverage now lives in `dx12_resource_tracker_tests.cpp` (6 tests). Both
`allow_promotion`'s `global`/`COMMON` gate and `allow_decay`'s `type_before == promoted` gate were
fault-injected and confirmed to fail tests when broken.

## Milestone 7c: geometry draw calls (complete)

Four public entry points on the draw device -- `draw_lines`, `draw_triangles`, `draw_rectangle`,
`draw_circle` -- plus the `shapes` sample mode that renders them in a window.

### Design differences from the C++ original

- **No nullable color sentinel.** The C++ endpoints carried `const ff::color*`, where null meant
  "inherit the previous point's color", with subtly different inheritance rules in `draw_lines`
  (falls back to the *first* point's color) and `draw_triangles` (falls back to the *previous*
  point's color). `ff_dx12_draw_endpoint` gives every point its own color instead. Callers that
  want a single-color shape fill in the same value, which costs nothing and removes an
  inconsistency that was easy to trip over.
- **Thickness is a plain float, not `std::optional<float>`.** Zero means filled. This is the same
  encoding the instance struct already used, so the optional was being flattened to zero anyway.
- **`outside_color` is a value, not a nullable pointer.** Callers pass the inside color again for
  a solid circle.

### A real bug in the C++ original, not ported

`draw_circle` with a negative thickness does `radius += thickness`. The C++ version only rejects
`thickness >= radius` *after* that adjustment, so a call like `radius 10, thickness -10` produces a
**zero radius** instance, and `-15` produces a **negative** one. The C port adds
`FF_CHECK_RET(radius > 0.0f)` after the adjustment. Pinned by
`circle_thickness_that_consumes_the_radius_draws_nothing`, which was confirmed to fail when the
check is removed.

### Depth: one slice per draw call, not per instance

All segments of a polyline and all triangles in one `draw_triangles` call share a single depth, so
they merge into one instanced draw. This matches the C++ behavior and is what keeps a 23-segment
gradient line at one draw call rather than 23. Separate calls still advance, or they could not sort
against each other. Pinned by `one_polyline_shares_a_depth_across_its_segments`.

### Alpha merging

A segment whose two endpoints disagree on alpha has to go through the transparent path even when
neither endpoint is translucent, because the shader interpolates between them and the in-between
pixels are translucent. `merge_alpha_type` collapses any disagreement to transparent.

**This produced the one vacuous test of the milestone.** The first version of
`a_line_with_mismatched_endpoint_alpha_is_transparent` put the translucent endpoint *second*, so
with the merge removed the last endpoint alone still decided "transparent" and the test passed
anyway. Reordering to translucent-then-opaque made it discriminating. The general trap: when
testing a fold or accumulate, the element that must survive the fold has to be positioned where a
naive "last value wins" implementation would drop it.

25 new tests, five faults injected one at a time, all caught after the reorder above.

### The `shapes` sample

Renders filled/outlined rectangles, a gradient circle, gradient triangles, a **closed** pentagram
polyline, and a 23-point polyline with varying thickness and color. The pentagram is deliberate:
a closed polyline is the only shape where the miter wrap at the seam has real neighbors on both
sides, so a bad wrap shows up as a visible notch.

Verified the batch really is built by temporarily logging bucket totals: 44 instances, 3
transparent, 1 interned matrix, which matches the shapes drawn exactly. Worth doing because a
malformed vertex buffer view silently draws nothing rather than failing, so "frames rendered" alone
proves very little.

Debug 1165/1165, Release 1165/1165. 60 fps, zero long frames, in both configurations.

## Milestone 7c addendum: the blank-frame bugs

Geometry drawing was implemented and 46 tests passed, yet the `shapes` sample rendered a
completely black window. Three independent defects were involved, and no existing test could see
any of them because every test stopped at "the GPU accepted the command list". A NULL or
mismatched vertex buffer binding is perfectly legal D3D12: it silently draws nothing.

1. **No render target, viewport, or scissor was ever bound.** `ff_dx12_commands_viewports`,
   `_scissors` and `_targets` existed but had no callers anywhere in the codebase; the C++
   `draw_device::internal_setup` binds all three. A default viewport is all zeros, so the
   rasterizer clipped every pixel. Now bound at the end of `ff_dx12_draw_device_begin`, which
   gained `target` and `target_view` parameters.

2. **Instance data was bound to the wrong input slot.** The input layouts declare instance
   elements in `FF_DX12_INSTANCE_SLOT` (1), but `apply_bucket` packed buffers from slot 0 upward,
   so every non-circle bucket bound its instance buffer to slot 0 and the shader read zeros.
   Circles were unaffected only by accident, since their per-vertex buffer already occupied
   slot 0. `FF_DX12_VERTEX_SLOT` / `FF_DX12_INSTANCE_SLOT` moved into `dx12_draw_state.h` so the
   layout and the binding cannot drift apart again, and `apply_bucket` now indexes its arrays by
   those constants instead of by a running counter.

3. **The projection matrix was not transposed.** Model matrices are transposed at intern time,
   but `update_constants` assigned `projection` raw. The shader does `mul(pos, mul(model,
   projection))` (row-vector) while HLSL cbuffers default to column-major, so the projection needs
   the same transpose. The C++ `setup_view_matrix` does exactly this. With only bug 2 fixed the
   test lit 528 of 4096 pixels; with the transpose restored it lit all 4096.

### Pixel readback is now the standard for "does it actually draw"

`a_filled_rectangle_actually_writes_pixels` draws a full-target white rectangle, copies the target
into a readback allocation and counts lit pixels. Recipe: allocate from
`ff_dx12_readback_allocator()` via `ff_dx12_mem_allocator_ring_alloc_buffer(...,
ff_dx12_commands_next_fence_value(&commands))`, fill a `D3D12_SUBRESOURCE_FOOTPRINT`, call
`ff_dx12_commands_readback_texture`, then `ff_dx12_queue_execute` + `ff_dx12_wait_for_idle`, then
read `ff_dx12_mem_range_cpu_data`. The tracker resolves the copy-source transition on close.

`ff_dx12_commands_readback_texture` requires a non-NULL `source_rect`; it returns silently when
one is not supplied. Passing NULL made the readback test report zero lit pixels no matter what,
which masked the real state of the fixes for several iterations. When a readback test reports
zero, first prove the readback itself works by clearing to a known non-black color and asserting
the clear is visible.

Future milestones that add draw types should add a readback test alongside the batching tests. The
batching tests verify what was recorded; only a readback verifies what the GPU produced.

## m7d-1: RGBA sprites

Sprites split from palettes. This slice covers `ff_dx12_sprite`, `ff_dx12_sprite_transform`,
`ff_dx12_draw_device_draw_sprite`, texture interning and the descriptor-table bind. Palettes moved
to m7d-2 because they need palette data types, hash-based interning and per-row texture upload that
do not exist in the C port yet.

### Matrix and texture indexes are interned together

`sprite_indexes` takes both a matrix slot and a texture slot before it decides whether to flush.
Both tables are per-flush, so interning one and then flushing to make room in the other silently
invalidates the index already handed out by the first. Taking both, then flushing once and retrying
both, is the only ordering that cannot leak a stale index into an instance.

### Flush is a no-op outside begin/end

`ff_dx12_draw_device_flush` early-outs on `FF_CHECK_RET(state == drawing)`. A test that means to
exercise a table-full flush must set up a real target, commands and `begin` first, or it silently
tests nothing. The first version of `a_full_texture_table_flushes_and_starts_over` got this wrong;
the assert it hit was the test premise being wrong, not the code.

### A fault survived because a field was zero

Fault-injecting the matrix shift in the `indexes` packing (16 instead of 24) did not fail any test,
because the only test covering the packing used matrix index 0, where both shifts agree.
`sprite_indexes_pack_texture_sampler_and_matrix` exists specifically to close that: it uses two
textures, a non-identity matrix and the linear sampler so every packed field is non-zero and
distinct. When a test pins a bit layout, no field may be zero.

### Sample app

`test/ff.test.c/test_sprites.c` draws a generated 2x2 atlas (solid, ring, checkerboard, gradient)
so a wrong uv rect shows as the wrong picture rather than a subtle shift. The texture is
`R8G8B8A8_UNORM`, so pixels are built from named components rather than a packed literal; the first
version used BGRA-ordered literals and drew orange where blue was intended. The orbit ring uses
`push_no_overlap` to collapse 48 translucent sprites into one instanced draw.

## Fixed: the intermittent "ring flake"

`ring_fails_instead_of_blocking_on_submitted_gpu_work` failed intermittently in full-suite runs and
always passed in isolation. The cause was in the test, not the allocator, but the test had silently
stopped testing anything.

`ff_dx12_fence_init` creates the D3D12 fence *already at* `initial_value` (`completed_value =
initial_value ? initial_value : 1`). The test built its gate fence with `initial_value = 1` and then
called `queue->Wait(gate, 1)`, which was already satisfied. The queue never stalled, so the "busy"
fence was free to complete at any moment. Whether the ring saw it as complete came down to timing
against the rest of the suite -- hence a flake.

Two failure modes, both bad:
- when the signal landed late, the assert passed for the wrong reason
- when it landed early, the wrap succeeded and the test failed

The fix holds the gate at `completed_value + 1` and asserts up front that the gate really is closed,
so the precondition can never silently rot again.

### Fault injection proved the repaired test

Replacing the no-block guard in `alloc_ring` with `ff_dx12_fence_value_wait` made the test **hang**,
which is exactly the mid-frame CPU/GPU stall it exists to prevent. Before the fix the same injection
returned instantly and the test still passed. A hang is the correct signal here: the test is now
holding real unfinished GPU work.

### Lesson

A test that sets up a GPU stall must **assert the stall exists** before relying on it. Otherwise the
setup can degrade into a no-op and the test keeps reporting green. `ff_dx12_fence_init`'s
"initial_value is already complete" behavior is now documented in `dx12_fence.h` because it is the
kind of off-by-one that produces a passing test rather than a compile error.

## Audit: GPU-stall test preconditions

Follow-up to the ring flake. Audited every test that sets up unfinished GPU work.

Exactly two tests stall a queue with `ID3D12CommandQueue::Wait`:
- `ring_fails_instead_of_blocking_on_submitted_gpu_work`
- `over_budget_does_not_evict_work_that_is_still_in_flight`

Both had bugs. Neither was a product bug; both were tests that could silently stop testing.

### 1. Hardcoded gate values

Both used a literal (`1` and `2`) for the gate. The descriptor one was off by one and never stalled
at all. The residency one happened to be right, but only because `initial_value` was `1` -- changing
that argument would have silently broken the stall while the test kept passing. Both now derive
`gate_closed` from `gate_fence.completed_value + 1` and assert `GetCompletedValue() < gate_closed`
immediately after the `Wait`, so the precondition is checked rather than assumed.

### 2. The vacuity re-check was itself racy

`over_budget_does_not_evict_work_that_is_still_in_flight` re-checked its precondition with
`ff_dx12_fence_value_complete(in_flight)`. That is a *GPU-side* observation: `in_flight` only becomes
complete once the driver processes the signal behind the gate. Opening the gate and re-running showed
the fault surviving 6/6 -- the CPU simply got there first. Adding a 300 ms sleep made the same fault
fail, confirming the check was a race rather than a guard.

Both tests now check `gate->GetCompletedValue()`, which is CPU-side and exact, instead of inferring
the gate state from downstream GPU work.

### Verification

Injecting the original off-by-one (`gate_closed = completed_value`) now fails **both** tests
deterministically, 3/3 runs, with "the gate must start closed" / "gate must still be holding the
queue". Full Debug suite 1170/1170 clean, three consecutive runs.

### Rule

Assert a precondition using the most direct, CPU-side observable available. Inferring "the GPU is
busy" from a fence the GPU still has to touch is a race, and it fails open -- the test passes.

## m7d-2: palette sprites

Palette sprites sample a texture of indexes rather than colors, run each index through a 256-entry
remap, then through a 256-entry palette row to get RGBA. This keeps classic paletted art cheap and
lets a sprite recolor or cycle without touching a pixel of the index texture.

New: `dx12_palette.h/.c` with `ff_dx12_palette_data` (an N-row RGBA texture plus a per-row hash),
`ff_dx12_palette` (a cursor onto one row), and `ff_dx12_palette_remap` (256 bytes plus a hash).

Draw device: two shared textures, 256 x MAX_PALETTES RGBA and 256 x MAX_PALETTE_REMAPS R8_UINT.
Palettes and remaps intern by hash per flush the same way matrices and textures do, so repeated
pushes of the same palette cost one row. Rows upload only when the cached hash for that slot
differs, so a static palette costs one copy for the life of the device rather than one per flush.
All four per-flush tables are interned before any flush, since flushing to make room in one would
invalidate an index already taken from another.

Notable details:
- A hash of zero is remapped to one; zero is the "slot never uploaded" sentinel in the row cache.
- `ps_constants_0` uploads whole, matching the old code. It is 512 bytes uploaded at most once per
  flush, so trimming it to the palette-texture count would save nothing measurable.
- A palette sprite with no palette pushed is dropped rather than silently using row 0.
- The index texture must be R8_UINT so the shader's `Load` returns the index unscaled.

Testing: four unit tests including a GPU readback that proves the full index -> remap -> palette
chain. The readback test initially passed with the remap deliberately broken, because the pushed
remap interned at row 0 and dropping the index was a no-op; it now interns the identity first so
the remap row is non-zero and load-bearing. Every packed field in the index-layout test is distinct
and non-zero for the same reason. A `palettes` mode was added to the sample app and verified by
screenshot, which is how the RGBA byte-order bug in the test palette was caught.

### Sample app: palettes mode covers every draw type

Geometry reaches palettes differently than sprites do. There is no palette-lookup pixel shader for
lines, triangles, rectangles or circles: `ps_color_out_palette` only *writes* an index. So geometry
uses palettes by rendering into an R8_UINT target with `ff_color_palette`, and that target is then
displayed as a palette sprite, which is where the index-to-color lookup happens.

The `palettes` mode is therefore two passes. Pass 1 draws rectangles (filled and outlined), circles
(outlined), a polyline spiral, a triangle fan, a palette sprite and an RGBA sprite into an offscreen
R8_UINT scene target, all carrying palette indexes rather than colors. Pass 2 draws that scene
target to the window as a palette sprite, alongside palette sprites that exercise row cycling, the
remap, and a no-overlap swarm. Choosing a different palette row when displaying the scene recolors
every geometry draw in it at once, which is the point of the indexed pipeline.

Covered by draw type: lines, triangles, rectangles (fill + outline), circles, `draw_sprite` into a
palette target (the text path, index from vertex color), and `draw_palette_sprite` both into a
palette target and into an RGBA target.

A `geometry_writes_its_palette_index_into_a_palette_target` readback test pins the geometry path:
it draws a rectangle with a distinct index and reads the R8_UINT target back to confirm that exact
index landed. Fault-injected with an off-by-one in `ff_color_to_shader`, which it catches.

## Palette remap audit

Compared against the old C++ `draw_util.cpp` / `palette_base.h`.

### Bug found and fixed: the remap never reached geometry

`store_color` passed `NULL` as `ff_color_to_shader`'s `index_remap`, so the CPU-side remap was
never applied. The old device applied it at every color store
(`to_shader_color(this->palette_remap())`).

This mattered because the two remap paths are disjoint, not redundant:

- **Texture-sourced indexes** (palette sprites) remap on the GPU, via
  `palette_remap_.Load(...)` in `ps_sprite.hlsl`. This path worked.
- **Vertex-color-sourced indexes** (lines, triangles, rectangles, circles drawn with
  `ff_color_palette` into an R8_UINT target) have no GPU remap at all: `ps_color_out_palette`
  writes the vertex color's index straight out. CPU-side was the only opportunity, so
  `push_palette_remap` was silently a no-op for all geometry.

Fixed by having `store_color` take the device and read
`palette_remap_stack[count - 1].remap`. Confirmed by
`a_pushed_remap_applies_to_geometry_drawn_into_a_palette_target`, which failed before the fix
(4096/4096 pixels kept the un-remapped index).

### Remapping still works as designed

`one_palette_with_two_remaps_produces_two_colors_in_one_frame` covers the feature the old
`palette_cycle` existed for: one palette's colors reused through several substitution tables in a
single frame. Each remap interns to its own row of the remap texture, so switching remaps costs a
packed index in the instance, not a flush. Fault-injected by forcing
`palette_remap_index_no_flush` to return 0; the test failed as it should.

### Deliberate divergence: palettes do not carry their own remap

The old `palette_base` had a virtual `remap()`, and `push_palette` pushed it onto the remap stack
unconditionally. `ff_dx12_palette` has no such field.

This is kept as-is. Bundling a remap into the palette only existed so `palette_cycle` could ship
data, row and remap as one object; the remap stack already expresses "one palette, many remaps"
directly, and keeping them independent avoids a push that silently affects state the caller did not
mention. If a resource type later wants to bundle them, it can push both.

### Still open

- `palette_row_hashes` / `palette_remap_row_hashes` are not cleared on device reset. The textures
  are recreated but the hashes persist, so a row that needs re-uploading could be skipped. Not
  reproduced yet; reset handling is not otherwise implemented in the draw device.

## Device reset for the draw device and palettes

The draw device and `ff_dx12_palette_data` were both missing from the `device_child` registry, so a
device reset left palettes permanently black. Two independent causes, each confirmed by fault
injection against `palettes_still_draw_after_a_device_reset`.

### ff_dx12_palette_data lost its colors

`ff_dx12_texture` rebuilds the resource and re-creates the SRV, but explicitly does not restore
pixels: "this layer never keeps a CPU copy, so the owner has to re-upload them". Nobody owned that
for a palette, so the palette texture came back cleared.

Fixed the way `ff_dx12_buffer` already handles `gpu_static`: the colors are copied into the
palette's own arena at init and re-uploaded in `internal_ff_dx12_palette_data_reset`.

### The draw device's row-hash caches went stale

`palette_row_hashes` / `palette_remap_row_hashes` exist to suppress redundant uploads: a row is
uploaded only when its hash differs from what is cached for that slot. After a reset the shared
palette and remap textures are blank but the caches still claim every row is correct, so nothing is
ever re-uploaded. `internal_ff_dx12_draw_device_reset` clears both.

Note these two fixes are not redundant. Clearing the caches alone still yields black, because the
source palette has nothing left to copy from; restoring the palette alone still yields black,
because the cache suppresses the copy. Injecting either one individually reproduces the failure.

### before_reset abandons the in-flight batch

Registering the draw device also gave it somewhere to drop a half-built batch. Its instance data
lives in upload memory the allocators are about to release, and the commands it would flush into
belong to the dying device, so `before_reset` clears the batch and returns the state machine to
`valid`. Without this a reset during a frame would flush into freed memory.

### Ordering

`palette_data` and `draw_device` are appended after `draw_state` in the child-type enum, so they
reset after the textures and views they depend on (forward order) and tear down before them
(reverse order).

## Frame pacing: blaming the app for vblanks it did not miss

The sample app ran at a steady 60fps, then every ~16 seconds burst past 200fps, jumped forward,
and settled again. The log named the culprit: `Frame pacing FAILS. stage 0 -> 1`. Stage 1 is
vsync-off, so the burst *was* the ladder's own response, not the problem it was reacting to.

The trigger was a run of ~11 frames at exactly two refresh intervals, recurring about every 960
frames. That beat is what a 60.00 vs 59.94 Hz mismatch looks like: the compositor drops a vblank.

Instrumenting the phases of `end_render` settled it. The wait on the latency handle was 33ms
while everything before it took 0.5ms, so the renderer was about 3% utilized and nowhere near
missing a deadline. No amount of dropping vsync can make a missing vblank arrive, so the ladder
was demoting for a condition it had no power to fix, and the backoff doubling (4, 8, 16) meant it
would keep rediscovering it forever.

The ladder now only blames a long frame on the app when the app was actually busy for it.
`end_render` measures the latency wait, subtracts it, and passes the remaining busy time to
`internal_ff_dx12_pacing_add_frame_busy`; a frame counts as late only when it is over budget *and*
busy time exceeded half a refresh. `add_frame` stays conservative for callers with no measurement
and charges the whole interval as busy.

Excused frames are not discarded. They accumulate in `total_idle_late_frames` and the test app
reports them separately, so a genuinely dropped vblank stays visible instead of being silently
swallowed by the thing that used to overreact to it.

Verified with 75-second runs of all three modes: zero stage changes, a steady ~59fps, and every
late frame attributed to the display rather than the renderer.

## Old-vs-new trace: every per-draw input to its consumer

The three things this port missed - device reset, palette remaps, sprite transparency - all had one
shape: state that existed and looked plumbed but reached no consumer. Reading the new code and
asking "is this correct?" cannot find them, because it always is, given what it can see. So this
pass went the other way: enumerate the old per-draw state and shader inputs, then find the line in
the new code that consumes each one. Anything terminating without a consumer is a miss.

All 47 members of the old `draw_device_base` now have a counterpart or a recorded decline.

Instance layouts match field for field: sprite (rect, uv_rect, color, pos_rot, indexes), line
(start, end, before_start, after_end, both colors, both thicknesses, depth, matrix_index), and the
triangle, rectangle and circle structs.

Draw state all reaches a decision: `force_opaque` to `allow_transparent`, `force_no_overlap` to
`nudge_depth`, `force_pre_multiplied_alpha` to `make_flags`, `ignore_rotation` to the view setup,
and `sprite.transparent` to `sprite_alpha_type`.

`nudge_depth` is identical to the original, including the rule that a no-overlap run shares one
depth so it can merge into a single instanced call.

Both constant buffer uploads match the original. An earlier revision of this document claimed two
of them were improvements on it; that was wrong in both cases and is corrected here.

`ps_constants_0` uploads the whole struct, and so did the old code - the old
`update_ps_constants_buffer_0` gated on `textures_using_palette_count` being non-zero but then
passed `sizeof(ps_constants_0)`. The count controlled *whether* to upload, not how much. This is
parity, not a fix. Uploading whole is still the right call, but only because the struct is 512
bytes and is uploaded at most once per flush, so trimming it would save nothing measurable.

`vs_constants_1` uploads exactly the slots interned this flush, and so did the old code in
release. The old size expression was `debug_build ? sizeof(struct) : sizeof(matrix) * count`,
which reads at a glance like debug and release disagreeing about correctness, but the release
branch is the real one and the new code does the same thing. The debug branch uploaded the full
array so that the D3D12 debug layer, which validates a CBV read against the resource size rather
than against the root CBV (which carries no size), would not report a false out-of-bounds read on
padding it could not know was unused. That is a debug-layer accommodation, not a behavior
difference. Uploading only the used slots is the efficient choice and both versions make it.

The out-of-range concern does not apply to either buffer. A matrix index reaches the shader only
through an instance field written by `matrix_index_no_flush`, which hands out indices below
`matrix_count` or nothing at all, so no instance can reference a slot that was not uploaded.

One dead input found: `view_scale_` in `data.hlsli` is declared and written but never read by any
shader. It is dead in the old code too, so this is inherited rather than a port miss. Left in
place because the constant buffer layout has to match the shader declaration.

Two deliberate declines. `push_custom_context` was a `std::function` escape hatch that nothing in
the engine used; games may want custom shaders later, but through a mechanism that fits this
layer. The world matrix stack is flattened to `set_world_matrix`, since the stack mostly existed
to hang a change signal off for cache invalidation, which the setter now does directly. Callers
that need nesting save and restore the matrix on the callstack.

## sprite_perf

The stress mode the depth and batching work was aimed at. Sprites are stored as 16 bytes each
(angle, radius, speed, cell) and their transforms are derived per frame rather than stored, so a
million sprites still fit in a 16 MB working set. Placement is seeded deterministically and only
the newly added tail is initialized when the count grows, so raising the count extends the scene
instead of reshuffling it and two runs at the same count are comparable.

SPACE doubles the count and BACK halves it, because finding the count where the frame rate breaks
takes a handful of presses that way instead of hundreds. DEL clears, P switches between RGBA and
palette sprites, N toggles no-overlap, T toggles a translucent tint, S pauses the motion without
pausing the loop. A third command line argument sets the starting count for non-interactive runs.

The status line reports build time per sprite, measured around the draw loop and `end_draw`
together - `end_draw` is where the batch becomes draw calls, so timing only the loop would credit
the batching for work it merely deferred.

Measured in Release: about 31-45 ns per sprite, flat from 5,000 to 1,000,000, so there is no
hidden quadratic in the batching. 5,000 sprites hold a locked 60fps at 0.025 cores. At 100,000 the
frame is GPU-bound on fill rate at 30fps while the CPU sits at 0.15 cores.

That 100,000 case also validates the pacing fix from the other direction. The earlier fix taught
the ladder to excuse a late frame when the app was idle; here the app genuinely is busy, and in
the Debug build the ladder correctly demotes to stage 1. Idle stalls are excused, real overruns
are still acted on.

## Milestone 8 design: resources

This is the layer that turns a name into a loaded thing, and it has to sit under textures and
sprites before either of those can be written. The goal is one API - ask for a resource by name -
that works whether the bytes came from a JSON file next to the exe or from a prebuilt pack where
all the parsing already happened, and that can swap a resource out underneath a running game when
the file on disk changes.

The old system is not being reimplemented. What follows keeps the two ideas from it that earned
their keep and drops the machinery around them.

### What the old design got right, and what to leave behind

Two ideas are worth keeping.

The first is the **source/cache split**. Every resource type implemented both `load_from_source`
(parse a PNG, build mips) and `load_from_cache` (map an already-converted blob). The same declared
resource can therefore come from either a readable authoring format or a fast prebuilt one, and
nothing above the factory knows which happened. This is exactly what was asked for and it is kept
almost unchanged.

The second is the **`new_resource` redirect** for hot reload. When a file changed, the old code did
not mutate the live resource; it built a whole new one and left a forwarding pointer on the old.
Anything still holding the old handle could follow the pointer and pick up the new value at a
moment of its choosing. That is the right shape, because it never mutates an object while a frame
may be mid-draw over it. It is kept, as an explicit generation counter rather than a chain of
`shared_ptr` forwards.

What is not being ported: the coroutine loader (`co_task`), the `shared_ptr`/`weak_ptr` graph, the
per-resource `win_event` and blocked-count deadlock tracking, `resource_objects` as itself a
savable resource, and the `resource_object_factory` virtual hierarchy. That machinery exists to
make *arbitrary* dependency graphs load concurrently while a caller blocks on any node. It is a
lot of moving parts for a problem a game mostly does not have: a level's resources are known up
front and can be loaded as a batch.

### The one discovery that shapes everything

`ff_idict` already does the hard part. `ff_idict_load` maps saved bytes **in place** - no parsing,
no allocation, and the comment at `idict.c:710` notes a mapped file stays paged out until a value
is actually read. It stores binary data, nested dicts, arrays, strings, rects and points, and
`ff_idict_save` writes it back with a hash.

So the pack file does not need a new format. **A resource pack is an `ff_idict` of name to
resource-dict, memory-mapped and used in place.** Booting a 200 MB pack costs one `MapViewOfFile`
and a header validation. Nothing is read until a resource is asked for, and a texture's pixels are
a `ff_ivalue_as_data` pointing straight into the mapping.

That also means source and cache converge on one shape. JSON parses into an `ff_dict`; a pack
gives an `ff_idict`. Both are "a dict describing one resource", so a loader reads its parameters
the same way from either, and the only real difference is which key it finds: a `file` to import,
or a `data` blob that was already converted.

### The design

Four pieces.

**`ff_resource_pack`** - a set of named resource dicts from one source. Init it from a mapped pack
file (`ff_idict_load`, zero copy) or from a JSON manifest (`ff_json_parse` into an arena-backed
`ff_dict`). Either way it answers "give me the dict named X" and reports whether that dict is
source-form or cache-form.

**`ff_resources`** - the search path. An ordered list of packs, searched front to back, so a loose
JSON manifest can shadow the shipped pack for one resource without rebuilding anything. This is
what a game actually holds, and the one type that needs a name-to-resource hash table.

**`ff_resource`** - a loaded object plus its identity: name, type tag, and the arena its contents
live in. Handed out as `ff_resource*`, stable for the lifetime of the load. Not reference counted;
ownership sits with `ff_resources`.

**`ff_resource_loader`** - a type tag plus two function pointers, `load_from_source` and
`load_from_cache`, registered in a table. This is the tagged-dispatch equivalent of the old factory
hierarchy, following `ff_stream`'s tagged union rather than a vtable. A loader is free to implement
only one of the two: a shader that can only be compiled offline leaves `load_from_source` null in
shipping builds and the pack is then mandatory for it.

Types get registered rather than hard-coded so `ff.base.c` does not gain a dependency on every
resource type. The DX12 texture loader registers itself; the resource core knows nothing about
DX12.

### Async, without coroutines

The old loader was async per resource, with each node able to block on its dependencies. The
replacement is async per *batch*: `ff_resources_load_all` walks the names, pushes each onto the
existing `ff_task` pool, and waits once for the group. `ff_task` already exists and already has a
flush.

This is a deliberate simplification and it is worth being honest that it gives something up: a
single resource requested mid-frame that is not yet loaded will block the caller. The bet is that
games load per level, not per frame, and a level's resource list is known before it starts. If
streaming ever becomes real, per-resource async can be added then, against a real use case rather
than a hypothetical one. The old design's deadlock-detection fields (`blocked_count`,
`parent_loading_infos`) are evidence of what the general version costs.

### Hot reload

Hot reload is where the device-reset lesson from the renderer applies directly: **never mutate a
live object while the GPU might be reading it.**

A debug-only watcher (`ReadDirectoryChangesW`) notices a file change and marks the affected
resource dirty. Nothing loads on the watcher thread. At a frame boundary the app drains the dirty
set - the same "between frames" point `ff_dx12_flush_deferred` already establishes - reloads into a
*new* `ff_resource` with a fresh arena, and leaves the old one holding a pointer to the new plus a
bumped generation. The old object's memory is released one full frame later, through the same
keep-alive discipline GPU resources use.

Holders notice via a cheap generation check rather than a signal, so a sprite holding a texture
does not need a subscription. The watcher and the whole dirty set compile out entirely in
shipping builds.

### How a texture actually loads

This is the case that motivated the milestone, and it is worth walking end to end.

*From source:* the dict has `file: "bricks.png"`. The loader maps the file, calls the existing
`ff_png_decode` into the resource's arena, optionally premultiplies alpha, and uploads. The decoded
pixels are kept, because a device reset needs to re-upload them without re-reading the disk.

*From cache:* the dict has a `data` blob. `ff_ivalue_as_data` hands back a pointer into the mapped
pack and the loader uploads straight from it, with no decode and no copy. The pixel memory is the
memory-mapped file, so a texture that is never reset never pages its pixels in beyond the upload.

Both paths end at the same place: an `ff_dx12_texture` plus a retained CPU copy. Device reset then
re-uploads from that copy - which is the missing piece `test_blit.c` currently works around by
re-uploading every single frame.

Premultiplied alpha, format conversion and mip generation stay on the *source* path only, so the
cache path is a straight upload. That is what makes the pack worth building, and it is why the
conversion tool is a separate offline concern rather than part of this milestone.

### What this milestone does not include

The pack *builder* is not part of it. Reading a pack and reading loose JSON both ship first, since
that is enough to run a game from source assets and enough to test both code paths (a test can
build a pack in memory with `ff_idict_save`, exactly as the PNG tests build PNGs with libpng's
writer). The offline tool that converts a source tree into a pack is worth its own milestone, and
DDS conversion without DirectXTex needs its own decision.

Also out: `sprite_list`, `sprite_font`, `animation`, and the `random_sprite`/`palette_cycle` types.
Those are content layers that sit on this one.

### Open questions

1. **Is per-batch async enough**, or is there a streaming case that needs per-resource waits?
2. **Should the cache path store DDS or a private format?** DDS is inspectable by external tools;
   a private layout is a struct plus a blob and avoids writing a DDS parser. Leaning private, given
   DirectXTex is off the table.
3. **Does a resource need to outlive its `ff_resources`?** Not reference counting is simpler and
   matches "a level owns its assets"; if a sprite can outlive the pack it came from, that changes.

## Milestone 8 design, part 2: preprocessing and packing

The refinement from the first design pass is that resources are looked up by **file name** -
`"foo.png"`, `"music/title.wav"`, `"ui.sprites"` - rather than by a name declared in a manifest. The
name is a path relative to the asset root, and it means the same thing whether the bytes come from
disk or from the pack. That removes the manifest indirection layer the old design had and makes the
"either source" requirement fall out naturally.

### The importer table

Preprocessing is a set of **importers** keyed by file extension. An importer reads one source file
and produces one `ff_dict` in parsed form - the same dict shape the runtime loader consumes from a
pack.

| Extension | Produces | Parsed form |
| --- | --- | --- |
| `.png` | texture | raw pixels blob, width, height, format, mip offsets |
| `.wav` | audio | decoded PCM blob, sample rate, channels, bit depth |
| `.sprites` | sprite sheet | the JSON inside, resolved: texture reference plus a sprite array of sub-rects, handles, and the transparency flag |
| `.json` | data | parsed dict, stored as-is |

Two things fall out of the extension being the key. A `.sprites` file is JSON *describing* sprites,
so its importer both parses that JSON and does the sub-rect transparency scan - which is where the
`sprite-transparency-flag` detection work finally lands, offline, where a full pixel scan costs
nothing at runtime. And an importer declares which *other* files it read (a `.sprites` file names a
`.png`), which is what makes dependency tracking possible.

The importer table is registered, like the loader table, so the resource core stays free of
dependencies on PNG, audio, or DX12.

### Two names per resource, not one

Each entry in the pack is keyed by source path, and importers may produce more than one resource
from one file. A `.sprites` file yields the sheet plus one entry per named sprite; the convention
is `ui.sprites` for the sheet and `ui.sprites:button_ok` for a sprite inside it. The colon cannot
appear in a path, so the two namespaces cannot collide.

### Building the pack

The builder is a standalone exe (`ff.resource.build`), following the old `ff.resource.build.exe`,
run from MSBuild. The old `build/cpp.targets` already has the shape for this with its `ResJson`
item type, `CustomBuild` with `MinimalRebuildFromTracking`, and `AdditionalInputs` pointing at
`%(RootDir)%(Directory)**\*` so any file under the asset directory retriggers the build. That
tracking-based incremental rebuild is worth keeping; it is what makes the "rebuild when any file
changes" part work without inventing a watcher for the build.

The builder walks the asset root, runs the importer for each recognized extension, and writes one
`ff_idict` via `ff_idict_save`. Alongside each resource it records a **manifest** entry:

- the source path, relative to the asset root
- its last write time and size
- the paths of every file the importer read

The dependency list is why `.sprites` matters: editing the `.png` a sheet refers to must
invalidate the sheet, not just the texture.

### Debug builds preferring newer files on disk

This is the mechanism asked for, and it is per-resource rather than per-pack. The old code had it
at whole-pack granularity (`load_cached_resources` rejected the entire cache if any input file was
newer), which means one edited PNG re-imports everything.

At startup a debug build reads only the manifest - cheap, since `ff_idict` maps in place and
nothing else is paged in - and stats each source file. A resource is **stale** if its source file's
write time or size differs from the manifest, or if any of its recorded dependencies differ. Stale
resources are marked; everything else loads from the pack as normal.

A stale resource loads by running its importer at runtime, in-process, against the file on disk.
The importer produces the same dict the pack would have held, so the loader below it cannot tell
the difference. This is the payoff of importers being a library the builder merely drives: the
debug path is not a second implementation, it is the same code.

Write time *and* size, rather than time alone, because copying files around preserves timestamps
often enough that size catches what time misses. A content hash would be stricter but requires
reading every file at startup, which defeats the purpose.

In shipping builds the manifest check compiles out entirely and the pack is trusted.

### The file-locking constraint

The old code carries a comment worth preserving: memory-mapping the pack **locks it on disk**, so
the build cannot overwrite it while the game holds it open. This directly conflicts with wanting to
rebuild the pack while a debug session is running.

The resolution is that debug builds do not map the pack; they read it into an arena. Shipping
builds map it, since nothing rebuilds underneath them. That costs debug startup time and saves the
ability to rebuild live, which is the right trade in that configuration. The zero-copy property
that made `ff_idict` attractive is preserved exactly where it matters - shipping - and
`ff_idict_load` works identically either way, since it only needs bytes.

### How this composes with hot reload

The staleness check is a startup-time version of the same question the watcher answers at runtime,
and they should share code: both produce "this resource is stale", and both resolve it by running
the importer in-process and swapping in a new `ff_resource` with a bumped generation. The watcher
is the incremental case, the startup scan the batch case.

### Order of work

1. Filesystem support: `ff.base.c` currently has no directory enumeration and no file timestamps.
   `ff_file_map`, `ff_stream` and `ff_file_module_dir` exist, but `FindFirstFileW` walking and
   `GetFileAttributesExW` are both needed and neither is written.
2. The importer table plus the PNG importer, which is the one with an existing decoder.
3. The builder exe and its MSBuild wiring.
4. The debug staleness check and runtime import.
5. The `.sprites` importer, which needs sprite types to exist first.

### Open questions

1. ~~**Is the asset root a single directory**, or a search path of several?~~ Answered in part 3:
   several, each under a module namespace.
2. **Should the builder be incremental internally**, caching per-file imports, or is MSBuild-level
   tracking plus a fast full rebuild enough? Full rebuild is far simpler and probably fine until
   the asset count is large.
3. **Do audio resources want decoding at all**, or should a `.wav` be stored compressed and decoded
   on demand? Music wants streaming, sound effects want decoded and resident.

## Milestone 8 design, part 3: module namespaces and the search path

The refinement here is that there is no single asset root. Several independent modules - `ff.base.c`
itself, the game, a third-party library - each register their own resources at startup, and each may
supply them as a directory of loose files during development or as a pack file in shipping. Names
are qualified by module: `base:foo.png` and `game:foo.png` are different resources that can coexist.

### Why namespaces rather than an ordered search path

The first design pass proposed an ordered list searched front to back, with earlier entries
shadowing later ones. That is the conventional answer and it is the wrong one here.

The old code is the evidence. `resource_objects::try_add_resource` (`resource_objects.cpp:266`)
merged every registered source into one flat map and, on a collision, did this:

```cpp
ff::log::write(ff::log::type::resource_load, "Duplicate resource: ", name);
return false;
```

First registration wins, later ones are dropped with a log line nobody reads. Two modules that each
ship a `button.png` produce a silent, **registration-order-dependent** bug: the game gets the
engine's button, or its own, depending on link order and init order. The failure is invisible until
someone notices the wrong art.

A namespace makes that case unrepresentable rather than merely detected. `base:button.png` and
`game:button.png` are simply different names, so there is nothing to resolve and no order to depend
on. This is the same reasoning that made `ui.sprites:button_ok` work in part 2, applied one level
up, and it costs a hash lookup on a shorter string rather than a walk down a list of packs.

It also gives a better error. An unqualified miss can say *"no module named `game`"* or *"`game` has
no `foo.png`"*, where a search path can only say "not found anywhere", which is the least useful
moment to have lost track of where you looked.

### The shape

**`ff_resource_module`** replaces the per-pack search entry. One module is a namespace plus an
ordered list of *sources*, where a source is either a directory on disk or a mapped pack. The
ordering is kept, but it is now **within** a module and means something specific: loose files in a
registered directory shadow the same name in that module's pack, which is exactly the hot-reload
story from part 2 - drop a file next to the exe and it wins over the shipped pack, but only for the
module that registered that directory.

**`ff_resources`** becomes a small map of namespace to module. Registration is
`ff_resources_add_module(resources, FF_SVL("game"), ...)`, and a duplicate namespace is a hard
failure at startup rather than a log line, because unlike a duplicate resource it is unambiguously a
programming error.

Lookup splits the name at the first `:` that precedes any `/`. That qualification is worth stating
precisely, because part 2 already spends `:` on sub-resources: `ui.sprites:button_ok`. A full name
is therefore `module:path:sub`, and the parse is "first colon splits the module, last colon splits
the sub-resource". Since a module namespace is constrained to `[A-Za-z0-9_]` - no colons, no slashes
- and a path may contain neither colons nor a sub-resource name containing one, the three-part split
is unambiguous. `base:ui.sprites:button_ok` reads correctly.

### What an unqualified name means

This is the one place worth being careful, because it is where a convenience feature can quietly
reintroduce the old bug.

An unqualified `foo.png` **does not search all modules**. Searching would be exactly the
order-dependent behavior the namespaces exist to eliminate. Instead each lookup context carries a
default module, and an unqualified name resolves against that one only. A game asking for `foo.png`
gets `game:foo.png` and nothing else; if it wants the engine's, it says `base:foo.png` and its
intent is in the source code rather than in link order.

The default is set per `ff_resources` handle rather than globally, so a library's own resource
lookups resolve to the library's module without the library having to spell its own name at every
call site.

### Consequences for the pack builder

Each module builds its own pack, independently, from its own asset root. That falls out of the
namespace being the unit of registration and it is a real simplification: the builder does not need
to know about other modules, there is no merge step, and a library can ship a prebuilt pack without
the game rebuilding it.

The manifest and staleness check from part 2 are per pack and therefore per module already, so
nothing there changes. The `ff_file_enumerate` work just finished is what walks one module's root.

Dependencies across modules are the one thing this opens up - a game `.sprites` file referring to
`base:atlas.png`. The importer records dependency paths already; those paths now need to be
qualified names, and a cross-module dependency means the builder cannot fully validate it in
isolation. The resolution is that the builder records the qualified name without resolving it and
the runtime resolves it on load, which is the only option that keeps module builds independent.

### Open questions

1. ~~**Can a module be registered after startup**?~~ Answered in part 4: not globally; locally yes.
2. **Should a module be allowed to reference another module's resources at all**, or should
   cross-module references be limited to explicitly exported names? Unrestricted is simpler; it also
   means any module can depend on any other module's internals.

## Milestone 8 design, part 4: registration lifetime and source-tree asset roots

Two decisions, and they turn out to be related: both are about keeping the shipping configuration
honest while making development convenient.

### Global registration closes after startup

The global set of modules is fixed once initialization finishes. Each module registers during its
own init - `ff.base.c` registers `base`, the game exe registers `game` - and after that the
namespace map is immutable.

Immutability is worth more than the flexibility it gives up. An immutable map needs no lock on the
lookup path, which matters because resource lookup happens from the game thread and potentially from
task threads at load time. It also makes the failure mode good: a missing module is a startup error
with every module present, rather than a mystery an hour into play when something registers late and
shadows a name.

What stays possible is a **local** `ff_resources`. Tests, the resource compiler, and a resource
viewer all want a private set of modules with no relation to the global one, and they get it by
initializing their own `ff_resources` instead of touching the global. That is the same type, just
not the global instance, so nothing extra is built for it. It also means the unit tests never mutate
process-wide state, which is what makes them safe to run in any order.

So the API splits: `ff_resources_init` / `ff_resources_add_module` are general and usable at any
time on any instance, while `ff_resources_global()` returns the one that seals after init. Asserting
that seal is cheap - a bool checked in `add_module` - and turns "registered a module too late" from
a subtle bug into an immediate failure.

### Asset directories in the source tree, not the output folder

This is possible, and it is better than copying.

The mechanism is that the compiler bakes the absolute source path into the binary as a define. The
build already computes `$(FFRoot)` in `build/base.props:18` and every project inherits
`build/cpp.targets`, so a project sets its asset directory and the targets file turns it into a
preprocessor definition:

```xml
<PreprocessorDefinitions Condition=" '$(Configuration)' != 'Release' And '$(FFAssetDir)' != '' ">
  FF_ASSET_DIR="$(FFAssetDir.Replace('\','/'))";%(PreprocessorDefinitions)
</PreprocessorDefinitions>
```

Forward slashes because the value passes through both MSBuild and a C string literal, where a
trailing backslash would escape the closing quote. Windows accepts forward slashes in paths
throughout, and `ff_file_enumerate` already normalizes entry names to `/` separators, so the two
conventions agree.

Verified this works as described: compiling with `/DFF_ASSET_DIR="\"C:/dev/x/assets/\""` and
printing the macro yields `[C:/dev/x/assets/]`. The path is a compile-time constant, so there is no
lookup cost and nothing to deploy.

`ff.base.c` therefore points at `source/ff.base.c/assets/` in its own source tree, the game points
at its own, and neither copies a single file into `bin`. Editing a PNG in the source tree is
immediately visible to a running debug build, which is the hot-reload story from part 2 working with
no build step at all - strictly better than copy-on-build, where the copy is what you would have to
wait for.

### Why this is safe to do only in development

The obvious objection is that a baked absolute path is meaningless on any other machine, and that is
exactly right - which is why the define is conditioned on `'$(Configuration)' != 'Release'`,
matching how `PROFILE` is already set at `cpp.targets:80`.

The two configurations genuinely differ:

- **Debug/profile**: the module's source is its asset *directory*, read as loose files. No pack is
  required, and the pack is consulted only if present. An absolute path into the source tree is
  correct because the binary only ever runs on the machine that built it.
- **Release**: the module's source is its *pack*, found next to the executable or embedded as a
  Win32 resource via the existing `ff_map_resource`. `FF_ASSET_DIR` is not defined at all, so a
  stray reference to it fails to compile rather than silently shipping a dead path.

Making it a compile error rather than a runtime fallback is the point. The alternative - define it
everywhere and fall back when the directory is missing - means a shipping build that quietly tries a
developer's path first, and a bug where a machine that happens to have that path works differently
from one that does not.

### What this means for the source ordering

Part 3 gave each module an ordered list of sources. That ordering now has a concrete default rather
than being a general mechanism looking for a use:

1. the source-tree asset directory, when `FF_ASSET_DIR` is defined (development only)
2. a pack file next to the executable, if present
3. an embedded pack resource, if the module has one

Development therefore prefers loose files and falls back to a pack; shipping has only the pack.
The same lookup code runs either way, and the only difference is how many sources the module was
initialized with.

### Open questions

1. **Does the game exe need its own asset directory at all for the test app**, or is a single
   `base` module enough until a real game project exists? The two-module case should be exercised by
   tests regardless, since it is the interesting one.
2. **Should `ff.base.c` ship any resources at all**? A default font and a white 1x1 texture are the
   usual candidates, and they are the reason the `base` module exists rather than the game owning
   everything.

## Sprite throughput: where 60fps actually breaks

Measured in Release on this machine, 1920x1080, vsync on, latency 1. Each number is a 12-second
run; the headline is **median frame time**, not the final one-second fps window, which turned out
to be too noisy to rank counts by (a 64k run reported 30fps while a 72k run reported 60, with both
showing a 16.7 ms median).

### 32x32 sprites

| Sprites | Median frame | p99 | Long frames | CPU cores |
| --- | --- | --- | --- | --- |
| 60,000 | 16.66 ms | 17.27 | 66 | 0.097 |
| 70,000 | 16.72 ms | 33.71 | 124 | 0.111 |
| 80,000 | 16.76 ms | 33.76 | 142 | 0.140 |
| 90,000 | 16.67 ms | 33.54 | 70 | 0.154 |
| 100,000 | 32.99 ms | 34.02 | 260 | 0.123 |

**60,000 is the last count that holds a solid 60fps.** At 70-90k the median is still 16.7 ms but
p99 has doubled to 33.7: most frames make it and a growing minority miss and wait a full extra
vblank. By 100k the median itself is 33 ms and it is a locked 30fps.

The failure mode is worth noting - it is not a gradual slope. Because vsync quantizes, a frame that
misses by a microsecond costs a whole 16.7 ms, so the transition from "fine" to "half rate" spans
about 40k sprites and shows up as p99 splitting away from the median long before the median moves.
p99 is the number to watch; by the time the median moves it is already too late.

### It is fill rate, not sprite count

CPU never exceeds 0.16 cores anywhere in that table, which already argues the CPU is not the
limit. Confirmed directly by shrinking the sprites from 32x32 to 8x8 - 16x less fill, identical
everything else:

| Sprites (8x8) | Median frame | CPU cores |
| --- | --- | --- |
| 100,000 | 16.67 ms | 0.198 |
| 200,000 | 16.67 ms | 0.339 |
| 400,000 | 16.55 ms | 0.669 |
| 600,000 | 16.62 ms | 0.830 |
| 800,000 | 21.66 ms | 0.926 |
| 1,000,000 | 28.16 ms | 0.934 |

400,000 small sprites hold 60fps where 100,000 large ones could not, so the limit tracks pixels
rather than instances. The 60k figure is about 61 Mpixel per frame, or 3.7 Gpixel/s sustained -
a plausible number for this GPU and one that will differ on another.

The small-sprite run also locates the *other* ceiling: throughput flattens around 600k sprites at
0.83 cores, and past that the CPU is saturated and the frame time climbs. So there are two
independent limits - roughly **60k sprites when fill-bound at 32x32**, and roughly **600k sprites
when CPU-bound at any size**.

### Per-sprite cost

Batch build time is 26-48 ns/sprite and does not trend upward with count (26.0 at 60k, 33.4 at
100k, 26.7 at 400k, 28.6 at 1M). Flat cost per sprite across a 16x range means no hidden quadratic
in the batching, interning, or bucket handling. The variance between runs at the same count is
larger than the variance across counts.

### What this means

For the game this library targets, 60,000 sprites of 32x32 at a locked 60fps is far more headroom
than a Robotron-class game needs; those peak in the low thousands. The practical conclusion is that
**sprite throughput is not the thing to optimize next**, and that when it eventually matters the
lever is overdraw and sprite size, not batch efficiency or draw call count.

Two caveats on these numbers. Every sprite here samples a 64x64 atlas with no overlap, so cache
behavior is ideal; real scenes with many distinct textures will flush the texture table more often
and land lower. And this is one GPU - the CPU-side figures should carry over, the fill-rate figure
should not.

## Would bindless push more sprites?

Short answer: not for the limit measured above, but yes for a limit not yet hit.

### What bindless changes

Bindless replaces descriptor tables with `ResourceDescriptorHeap[]` indexing in the shader. The
groundwork is already in (`ff_dx12_descriptor_range_heap_index`, `ff_dx12_supports_bindless`), so
adopting it is a real option rather than a rewrite.

What it removes is **CPU-side binding cost**: copying descriptors into a per-flush table, and the
flush itself when a table fills. What it does not touch is the work of shading a pixel.

### Why it will not raise the 60k figure

The 60k ceiling measured above is fill rate. CPU sat at 0.097-0.16 cores through that entire sweep,
and the decisive evidence is that shrinking sprites 16x raised the ceiling to 400k with no code
change at all. The GPU is spending its time writing pixels, and bindless does not make a pixel
cheaper to write.

Spending CPU time to buy more CPU headroom does not help when 84% of a core is already idle. The
one number bindless would improve - draw submission - is not in the critical path here.

This is worth being blunt about because the plan's own note at "Bindless groundwork" says bindless
is "for better AMD performance", which is a reasonable general belief but does not survive contact
with these particular measurements.

### The hardware detail that matters

This machine is an **AMD Radeon 860M**, an integrated GPU sharing system memory. That is exactly
the profile where fill rate binds early: no dedicated VRAM bandwidth, and a 1080p frame at 60k
sprites of 32x32 is ~61 Mpixel/frame of mostly-overdraw blending. A discrete card would move the
fill ceiling up substantially and the CPU ceiling hardly at all - which would make bindless
*relatively* more interesting on a discrete card, but only after the fill limit stopped being the
binding one.

### Where bindless would genuinely win

There is a real limit it addresses, and the perf test cannot see it: `FF_DX12_MAX_TEXTURES` is 32.
When a batch needs a 33rd distinct texture, `texture_index_no_flush` returns invalid and the device
flushes mid-batch - ending the instanced draw, rebinding, and starting over.

The stress test draws every sprite from one 64x64 atlas, so it never flushes once. A real scene
with hundreds of distinct textures would flush constantly, and *that* cost scales with texture
variety rather than pixel count. Bindless removes the cap entirely: with a persistent heap, a
sprite's texture is an index in its instance data and no table ever fills.

So the honest framing is that bindless is not a throughput optimization for this renderer, it is a
**scene-complexity optimization**. It converts "60k sprites sharing <=32 textures" into "60k sprites
sharing any number of textures", which is a different and more useful kind of headroom.

### Recommendation

Not yet, and not for speed. Two reasons to wait:

1. The fill limit binds first on this hardware, so bindless would measure as no change.
2. There is no benchmark today that can show a win, because nothing exceeds 32 textures. Building
   bindless before the test that proves it works means guessing.

The right order is: get the sprite resource pipeline in (M8), which is what makes many distinct
textures possible at all; extend `sprite_perf` with a texture-variety axis that can force table
flushes; confirm the flush cost is real and measurable; then implement bindless against that
benchmark. That also follows the rule that has caught the real bugs in this port - measure the old
behavior, then prove the new code changes the number.
