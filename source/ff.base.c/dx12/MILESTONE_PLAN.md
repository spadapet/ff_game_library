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
7. `dx12-descriptor-range` (future, milestone 2)
8. `dx12-descriptor-allocator` (future, milestone 2)
9. `dx12-resource-state` (future, milestone 2)
10. `dx12-resource-tracker` (future, milestone 2)
11. `dx12-resource` (future, milestone 2)
12. `dx12-queue` (future, milestone 3)
13. `dx12-commands` (future, milestone 3)
14. `dx12-object-cache` (future, milestone 3)
15. `dx12-buffer` (future, milestone 4)
16. `dx12-texture` (future, milestone 4)
17. `dx12-depth` (future, milestone 4)
18. `dx12-target` (future, milestone 4)
19. `dx12-shader-delivery` (future, milestone 5)
20. `dx12-draw-device` (future, milestone 5)

## Milestone 1 (current focus)

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

## Future milestones (2-5)

Milestone 2: descriptor allocation + resource state tracking.
Milestone 3: command queues/lists, object cache.
Milestone 4: concrete buffer/texture/depth/target resource types.
Milestone 5: shader delivery + draw device (the high-level rendering API).

These are listed for roadmap completeness only; no implementation work has
started on them yet.
