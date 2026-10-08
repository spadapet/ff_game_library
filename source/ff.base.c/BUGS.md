# ff.base.c bug tracker

This file records actionable findings from the 2026-09-30 review of `ff.base.c`, including comparison of DX12 behavior with `source\ff.application\graphics`. It is a persistent backlog, not a claim that the findings have already been fixed.

The initial review reported 24 numbered findings. The two independent arena-retention defects grouped in its DX12 allocation finding are tracked separately here, giving 25 initial entries.

## Baseline and evidence

- Recorded: 2026-09-30.
- Source baseline when this document was created: `e5c3c0f017cb9cf0a19b4dad58ebe674fadea754` (`Sprite perf fixes`).
- Initial evidence level: **source-reviewed**. The triggers below are derived from control flow, ownership, API contracts, and arithmetic; they are not reports of executed runtime reproductions.
- All initial entries were **Open**. No fixes were made as part of the initial review or documentation; subsequent resolutions are recorded below.
- Paths in findings are relative to `source\ff.base.c` unless explicitly repository-relative. Line numbers describe the baseline and will drift; use the named functions as durable anchors.
- A matching defect in the C++ implementation is marked **Inherited**. Functional parity does not make that behavior correct. **Port regression** is used only where the comparison established a relevant difference; otherwise origin is left unclassified.

## Tracking conventions

Use permanent IDs in commit messages and related issues. Do not renumber or reuse IDs. Add new entries with the next unused ID, currently **FFC-029**, and update the index. Keep resolved entries as history.

**Priority:** P1 = high-priority corruption, lifetime, synchronization, or deadlock defect; P2 = other concrete correctness, recovery, or sustained resource/performance defect. Priority is not a security severity rating.

**Status:** Open, In progress, Deferred, Fixed, or Not a bug. Deferred entries need a reason. Fixed entries need regression evidence and a resolution commit, or an explicit uncommitted-working-tree designation until committed. Not a bug entries need the contract or counterexample that disproves the finding.

**Evidence:** Source-reviewed, Reproduced, or Disproved. Keep evidence separate from status. A proposed regression scenario is not an executed reproduction.

When resolving an entry, record the owner if useful, the actual trigger, affected configurations, root cause, resolution, regression coverage, and any compatibility decision. Update both its status and the index. The reproduction suggestions are starting points, not complete test harnesses; deadlock/GPU-gate cases need bounded waits and reliable cleanup. Exercise Debug and Release when assertions or guards are involved.

## Index

| ID | Priority | Status | Finding |
| --- | --- | --- | --- |
| FFC-001 | P1 | Fixed | Only the first reader waits for the preceding GPU writer |
| FFC-002 | P1 | Fixed | Multiple open trackers retain destroyed resource wrappers |
| FFC-003 | P2 | Fixed | Whole-resource barriers ignore divergent fallback states |
| FFC-004 | P2 | Fixed | Caller-created allocators are omitted from device recovery |
| FFC-005 | P2 | Fixed | Ring metadata exhaustion causes exponential heap growth |
| FFC-006 | P2 | Fixed | Allocator pruning retains the oldest, smallest heap |
| FFC-007 | P2 | Fixed | Recycled free-list nodes abandon arena-backed metadata |
| FFC-008 | P2 | Open | Recycled keep-alive nodes abandon spilled fence storage |
| FFC-009 | P2 | Open | Filled circles ignore their inside color |
| FFC-010 | P2 | Open | Palette sprites use base dimensions for nonzero-mip views |
| FFC-011 | P2 | Open | Mip render targets report base-level dimensions |
| FFC-012 | P2 | Open | Array-slice sprite SRVs do not match shader dimensions |
| FFC-013 | P2 | Open | Depthless batching changes draw order |
| FFC-014 | P1 | Fixed | App shutdown deadlocks on synchronous main dispatch |
| FFC-015 | P2 | Fixed | Dictionary mutation corrupts borrowed input values |
| FFC-016 | P2 | Open | Embedding immutable dictionaries breaks payload alignment |
| FFC-017 | P2 | Open | Embedding an empty immutable dictionary omits its header |
| FFC-018 | P2 | Open | String-builder insertion corrupts aliased source text |
| FFC-019 | P2 | Fixed | Dictionary producers and consumers disagree on depth limits |
| FFC-020 | P2 | Fixed | Log sink configuration changes deadlock inside callbacks |
| FFC-021 | P2 | Fixed | JSON decimal parsing depends on the numeric locale |
| FFC-022 | P2 | Not a bug | Degenerate rectangles can intersect |
| FFC-023 | P2 | Open | Queued fullscreen requests discard the final desired state |
| FFC-024 | P2 | Open | Window placement mixes workspace and screen coordinates |
| FFC-025 | P2 | Open | Message-window class registration races across threads |
| FFC-026 | P1 | Fixed | Forgotten-resource barriers inherit replacement-resource state |
| FFC-027 | P2 | Fixed | Pipeline cache rejects embedded root signatures |
| FFC-028 | P2 | Fixed | Incomplete format metadata rejects valid textures |

## DX12 synchronization, lifetime, and recovery

### FFC-001: Only the first reader waits for the preceding GPU writer

**Priority:** P1. **Status:** Fixed. **Evidence:** Source-reviewed. **Origin:** Inherited.

**Location:** `dx12\dx12_resource.c:297-303`, `ff_dx12_resource_prepare_state`.

**Cause:** The read path adds `global_write` to one command list's dependency set and immediately clears it. Subsequent readers on other queues do not inherit the producer dependency. Residency's use fences prevent eviction; they do not order ordinary accesses to an already-resident resource.

**Trigger:** Submit a buffer upload on the copy queue without waiting on the CPU. Record/submit a direct-queue read, then a compute-queue read. The direct reader consumes the copy fence; the compute reader sees an empty `global_write`. CPU submission order does not serialize different GPU queues.

**Impact:** The compute reader can access incomplete or stale buffer contents while the upload is still running.

**Fix direction:** Preserve the last-writer dependency for every dependent reader, while retaining appropriate completed-fence pruning, deduplication, and same-queue handling. Do not replace the missing dependency with a global CPU stall.

**Regression scenario:** Hold the copy producer behind a controllable GPU gate. Submit readers on both other queues and verify neither can finish its dependent work before releasing the gate. Also inspect both readers' dependency sets. Ensure the harness releases the gate on every exit path.

**Parity evidence:** Repository-relative `source\ff.application\graphics\dx12\resource.cpp:272-278` also consumes/clears `global_write_` on the first read.

**Resolution:** Working tree, not committed. Read preparations now add the last writer fence without clearing it. Write preparations still wait on both that writer and accumulated readers before replacing the writer fence.

**Regression coverage:** `dx12_resource_tests::each_reader_waits_for_the_last_writer` passed in Debug/x64 and Release/x64. It verifies that two readers each inherit the pending writer dependency and that a following writer waits for the prior writer and both readers.

### FFC-002: Multiple open trackers retain destroyed resource wrappers

**Priority:** P1. **Status:** Fixed. **Evidence:** Source-reviewed. **Origin:** Unclassified.

**Location:** `dx12\dx12_resource.c`, `ff_dx12_resource_destroy` and `internal_ff_dx12_resource_before_reset`; `dx12\dx12_globals.c`, `internal_ff_dx12_forget_resource`; `dx12\dx12_queue.c`, `ff_dx12_queue_forget_resource`; `dx12\dx12_resource_tracker.c`, `ff_dx12_resource_tracker_forget`.

**Cause:** The original single `tracker` back-pointer could not represent multiple open command lists containing the same resource. Destruction therefore forgot only one tracker, and resetting a tracker could clear a pointer still needed by another.

**Trigger:** Explicitly record the same resource in two open lists, destroy its wrapper, and then submit the lists. Alternatively, submit the first list while the second remains open, destroy the wrapper, and submit the second. Allowing caller-owned wrapper storage to be freed or reused makes the stale reference observable.

**Impact:** A surviving tracker dereferences destroyed, freed, or reused wrapper storage. Keeping the underlying COM resource alive does not keep the wrapper alive; outcomes include use-after-free and barriers derived from the wrong resource state.

**Fix direction:** Track every recording reference, or otherwise detach every referencing tracker before wrapper destruction. Tracker reset must not erase another tracker's ownership information. Retain the existing deferred GPU-resource lifetime guarantees.

**Regression scenario:** Establish that both trackers actually contain the resource before destruction, then exercise destruction and submission in both orders above. Reuse the old wrapper storage to make accidental stale access visible. Do not rely on two identical `ff_dx12_buffer_update` calls: its size/hash shortcut can skip the second update entirely, creating no second tracker reference.

**Resolution:** Working tree, not committed. Resource destruction and device reset scan active command caches on the copy, compute, and direct queues, then forget matching entries through each tracker's existing resource index. This removes per-entry intrusive reference nodes and requires no tracker-reset unlinking. Forgotten state retains the native resource and is carried across tracker close chains, including an intermediate tracker that never referenced the resource.

**Regression coverage:** `dx12_lifetime_deep_tests::resource_destroy_scans_active_trackers_across_queues` and `dx12_resource_tracker_tests::destroyed_resource_state_flows_through_unreferencing_trackers` cover multi-queue cleanup, wrapper-storage reuse, and forgotten barrier propagation.

### FFC-003: Whole-resource barriers ignore divergent fallback states

**Priority:** P2. **Status:** Fixed. **Evidence:** Regression-tested in the C and legacy C++ trackers. **Origin:** Inherited.

**Location:** `dx12\dx12_resource_tracker.c:87-111,550-555,617-625`; `dx12\dx12_resource_state.c:204-216`; legacy `source\ff.application\graphics\dx12\resource_tracker.cpp:97-120,151-160`.

**Cause:** Whole-resource barrier resolution checks whether the previous tracker's stored entries are uniform. Uniform `none` entries mean "inherit global state", not "the effective states are uniform". The inherited global states can differ between subresources.

**Trigger:** Establish a two-mip texture with mip 0 in `COPY_DEST` and mip 1 in `PIXEL_SHADER_RESOURCE`, using a real transition for mip 1 so it does not merely promote and decay. In the next batch, list A redundantly requests `COPY_DEST` for mip 0; its resolved tracker state can collapse to uniform `none`. List B requests a whole-resource transition. B treats the effective previous state as uniform and can use mip 0's state for every mip.

**Impact:** An `ALL_SUBRESOURCES` transition has an incorrect `StateBefore` for at least one mip, violating D3D12 state tracking and potentially invalidating execution.

**Fix direction:** Determine uniformity from effective states after fallback resolution. Split barriers per subresource whenever those effective states differ. Review the analogous forgotten-entry resolution path as well.

**Regression scenario:** Exercise the sequence above and inspect emitted barrier before-states, not just the wrapper's final state. Final bookkeeping can look correct even when the emitted transition is invalid.

**Resolution:** Both trackers now test effective per-subresource states, including fallback state and promotion type, before keeping a whole-resource barrier. The C tracker applies the same resolution to forgotten entries and their saved global-state fallback.

**Regression coverage:** `dx12_resource_tracker_tests::all_subresources_barrier_splits_against_divergent_global_fallback`, `dx12_resource_tracker_tests::forgotten_all_subresources_barrier_splits_against_divergent_fallback`, and `resource_tracker_tests::resource_tracker_splits_all_subresources_against_divergent_global_fallback` inspect the emitted barriers and each subresource's before-state.

### FFC-004: Caller-created allocators are omitted from device recovery

**Priority:** P2. **Status:** Fixed. **Evidence:** Regression-tested in Debug and Release. **Origin:** Port regression.

**Location:** `dx12\dx12_device_child.h`, device-owner tags; `dx12\dx12_reset.c`, `child_before_reset` and `child_reset`; allocator and queue init/destroy paths.

**Cause:** Public constructors permitted caller-owned memory allocators, descriptor allocators, and queues, but reset rebuilt only the shared instances owned by `dx12_globals.c`. Registered resources could therefore be rebuilt against heaps or queues that still belonged to the prior device.

**Trigger:** Keep a placed resource backed by a caller-created allocator alive across actual device removal or migration to a different adapter. Recovery attempts to recreate the registered resource without rebuilding its local heap.

**Impact:** Old-device references can obstruct removed-device recovery. A heap belonging to the old adapter cannot be passed to the new adapter's device to recreate a placed resource.

**Important qualification:** Actual device removal and adapter migration are not emulated by the unit test. It pins references to each old local COM object so reset hooks must replace those objects even if D3D12 reuses the same healthy device instance.

**Resolution:** Memory allocators, CPU/GPU descriptor allocators, and command queues now register as device children when initialized and unregister when destroyed. The ordered reset walk rebuilds all live instances, including shared globals, so resources are reset only after their allocators and descriptors are available; queues are rebuilt after resource reset work. The duplicate global-only allocator reset walk was removed.

**Regression coverage:** `dx12_reset_tests::caller_created_device_owners_rebuild_on_reset` keeps caller-created memory/descriptor heaps and a queue alive across reset, pins references to the old COM objects to rule out pointer reuse, and verifies the new owners and placed resource use the current device. The test also confirms CPU/GPU descriptor ranges remain valid.

**Parity evidence:** Repository-relative `source\ff.application\graphics\dx12\heap.cpp:24-31` registers each C++ heap as a device child. See also the singleton/removal behavior in [D3D12CreateDevice documentation](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-d3d12createdevice).

## DX12 allocation and recycling

### FFC-005: Ring metadata exhaustion causes exponential heap growth

**Priority:** P2. **Status:** Fixed. **Evidence:** Regression-tested in Debug and Release. **Origin:** Port regression.

**Location:** `dx12\dx12_mem_allocator.c:36-87,298-354,461-539`; `dx12\dx12_mem_allocator.h:14,36-42`.

**Cause:** A ring buffer has 64 fence-range records. Exhausting those records returns the same failure as exhausting bytes, so the outer allocator creates a larger heap. Its growth policy doubles the previous heap size. The allocation path removes completed ranges only when the requested bytes overlap them, not merely because metadata is full.

**Trigger:** Make 513 allocations of 256 bytes, each with a distinct fence value, with no intervening `frame_complete`. Start with the global upload allocator's 1-MiB initial size. The 65th, 129th, and subsequent metadata-overflow allocations each cause another doubled heap. This can happen even when the stored fences are already complete.

**Impact:** The requested heaps total 1 + 2 + 4 + 8 + 16 + 32 + 64 + 128 + 256 = **511 MiB** for 131,328 bytes of payload, assuming creation succeeds. Larger sequences can fail allocation despite minimal useful data. The record count is bounded by distinct command/fence allocations, not just frames in flight.

**Resolution:** Ring fence ranges now use a dynamically growing circular array independent of heap bytes. Completed ranges at the front are retired before each allocation; incomplete fence values are never waited on or discarded. Range storage and its capacity survive buffer pruning and reuse.

**Regression coverage:** `dx12_mem_allocator_tests::ring_allocator_grows_metadata_without_growing_the_heap`, `ring_allocator_retires_completed_metadata_before_growing`, and `ring_allocator_reuses_grown_metadata_when_pruning_buffers` assert bounded heap capacity, completed-range reclamation, and metadata reuse for pending ranges.

**Parity evidence:** Repository-relative `source\ff.application\graphics\dx12\mem_allocator.h:58` uses a dynamically sized range list; `mem_allocator.cpp:130-139` adds records without treating metadata exhaustion as heap exhaustion. The descriptor ring in `dx12\dx12_descriptor_allocator.c:255-285` uses a growable circular array.

### FFC-006: Allocator pruning retains the oldest, smallest heap

**Priority:** P2. **Status:** Fixed. **Evidence:** Regression-tested in Debug and Release. **Origin:** Port regression.

**Location:** `dx12\dx12_mem_allocator.c:462-463,482-495`, buffer insertion and `ff_dx12_mem_allocator_frame_complete`.

**Cause:** Buffers are inserted newest-first, but pruning removed empty buffers starting at the head while more than one remained. This retained the oldest heap rather than the newest warm heap.

**Trigger:** Grow an allocator to a newest-first list of 4-MiB, 2-MiB, and 1-MiB buffers. Retire/free all allocations and complete the frame.

**Impact:** Pruning retains the 1-MiB heap, not the 4-MiB heap. Repeated bursts recreate larger heaps, adding allocation and residency churn rather than converging on the warmed-up workload size.

**Resolution:** Frame completion now retires completed ranges in every buffer, keeps the newest list head, and prunes eligible empty buffers from the remaining list. Recycled nodes retain stable addresses.

**Regression coverage:** `dx12_mem_allocator_tests::free_list_pruning_keeps_newest_heap_and_reuses_metadata` grows two free-list heaps, drains and prunes them, and asserts that the newest heap remains active.

**Parity evidence:** Repository-relative `source\ff.application\graphics\dx12\mem_allocator.cpp:298-320` stores buffers oldest-first, so its removal order preserves the newest when all are empty.

### FFC-007: Recycled free-list nodes abandon arena-backed metadata

**Priority:** P2. **Status:** Fixed. **Evidence:** Regression-tested in Debug and Release. **Origin:** Port regression.

**Location:** `dx12\dx12_mem_allocator.c:64-66,90-91,444-447,488-495`, free-list initialization, destruction, and pruning.

**Cause:** Each free-range array is allocated from the outer allocator's arena. Pruning destroyed and zeroed the buffer, losing the array pointer, then recycled only the outer node. Reinitialization allocated a new array; the old array could not be reclaimed until the allocator's entire arena was destroyed.

**Trigger:** Repeatedly grow a free-list allocator, free its allocations, prune extra buffers, and reuse the recycled nodes. Fragmented workloads grow the abandoned arrays further.

**Impact:** CPU memory usage grows with the number of recycle cycles even when live heaps and buffer nodes remain bounded. This is not merely normal arena retention of reusable capacity: the capacity has become unreachable.

**Resolution:** Pruning now saves and restores each free-list array pointer around buffer destruction. Reinitialization resets its single free extent in place, preserving the array's existing capacity. This remains independent of FFC-006: pruning can occur for any eligible non-head buffer.

**Regression coverage:** `dx12_mem_allocator_tests::free_list_pruning_keeps_newest_heap_and_reuses_metadata` fragments the old heap until its metadata grows, coalesces it, prunes it, then reuses the node for a larger heap and checks that the array pointer and capacity are unchanged.

### FFC-008: Recycled keep-alive nodes abandon spilled fence storage

**Priority:** P2. **Status:** Open. **Evidence:** Source-reviewed. **Origin:** Port-specific arena-lifetime issue.

**Location:** `dx12\dx12_globals.c:486-500,510-534`, `ff_dx12_keep_alive_resource`; `dx12\dx12_fence_values.c:13-24,56-69,95-104`.

**Cause:** Every call builds a fresh temporary fence set against `s_keep_alive_arena`. More than eight incomplete, distinct fences require spill storage. Assigning the temporary into a recycled node overwrites the node's previous spill pointer/capacity. `ff_dx12_fence_values_clear` could preserve that backing storage, but the subsequent whole-struct assignment defeats reuse.

**Trigger:** Submit more than eight distinct reader command lists referencing a resource while keeping their GPU completion pending, then destroy the wrapper. Let the work retire and flush the keep-alive list. Repeat using recycled nodes. Submit the readers before destroying the wrapper to isolate this issue from FFC-002.

**Impact:** Each spilled retirement set leaves another allocation in the global keep-alive arena until device teardown, despite bounded active keep-alive nodes.

**Fix direction:** Refill node-owned fence storage rather than constructing and assigning fresh spilled sets. Preserve deep-copy ownership so caller arenas may still be destroyed safely.

**Regression scenario:** Repeatedly retire resources with at least nine incomplete reader fences, then drain them. Confirm arena use stabilizes after warm-up. Also cover the immediate-release path and sets that fit inline.

## DX12 rendering

### FFC-009: Filled circles ignore their inside color

**Priority:** P2. **Status:** Open. **Evidence:** Source-reviewed. **Origin:** Inherited.

**Location:** `dx12\shaders\vs_circle.hlsl:16-20`; `dx12\dx12_draw_device.h:43,62-63`; circle index generation in `dx12\dx12_draw_device.c`.

**Cause:** The center vertex index is 64. The shader computes `(vertex_id >> 5) & 1`; for 64 this is zero, selecting `outside_color`, just as for the perimeter vertices 0-31. The inner outline ring occupies 32-63 and is a separate case.

**Trigger:** Draw a filled circle with different inside and outside colors. A particularly visible case is an opaque inside color and transparent outside color.

**Impact:** The filled circle has the outside color throughout instead of a center-to-edge gradient. The opaque-center/transparent-edge case can disappear entirely.

**Fix direction:** Classify the center vertex as inside without breaking the outline's inner/outer rings or radius calculations.

**Regression scenario:** Compare center and edge pixels for a filled gradient circle, and also cover outlined circles and transparent edges.

**Parity evidence:** Repository-relative `source\ff.application\assets\shaders\vs_circle.hlsl:16` has the same expression; the original renderer also uses center vertex 64.

### FFC-010: Palette sprites use base dimensions for nonzero-mip views

**Priority:** P2. **Status:** Open. **Evidence:** Source-reviewed. **Origin:** Inherited.

**Location:** `dx12\dx12_draw_device.c:655-662`, `update_ps_constants`; `dx12\shaders\ps_sprite.hlsl:36,78`; SRV mip selection in `dx12\dx12_resource.c:340-344`.

**Cause:** The uploaded palette texture dimensions come from the underlying texture, ignoring the view's `mip_start`. The shader converts normalized UVs to texel coordinates using those dimensions, then loads mip zero relative to the SRV.

**Trigger:** Use a 64x64 `R8_UINT` texture through a view beginning at mip 1. The selected mip is 32x32. UV 0.75 produces coordinate 48 instead of 24.

**Impact:** Palette sprites sample the wrong texels or read outside the selected mip, producing scaling/cropping errors and transparent zero-index regions.

**Fix direction:** Supply dimensions for the view's first mip, clamped to at least one texel per dimension, consistently in both palette-output paths.

**Regression scenario:** Draw identifiable palette patterns using mip-0 and nonzero-mip views. Cover both RGBA and palette-index render targets, including the smallest mip.

**Parity evidence:** Repository-relative `source\ff.application\graphics\dxgi\draw_util.cpp:839-845` likewise uses underlying texture dimensions.

### FFC-011: Mip render targets report base-level dimensions

**Priority:** P2. **Status:** Open. **Evidence:** Source-reviewed. **Origin:** Inherited.

**Location:** `dx12\dx12_target_texture.c:82-91`, `ff_dx12_target_texture_width` and `ff_dx12_target_texture_height`.

**Cause:** The getters return the underlying texture dimensions without applying `target->mip_level`, although the RTV selects that mip.

**Trigger:** Create a two-mip 64x64 texture and a target selecting mip 1. Its actual renderable extent is 32x32, while both getters report 64.

**Impact:** Viewports, projections, and other extent-dependent operations derived from the target API can be incorrectly scaled or cropped.

**Fix direction:** Return the selected mip's dimensions, clamping each dimension to at least one. Preserve base-level behavior.

**Regression scenario:** Assert dimensions and rendered coverage for base and nonzero mip targets, including nonsquare textures and one-texel dimensions.

**Parity evidence:** Repository-relative `source\ff.application\graphics\dx12\target_texture.cpp:121-125` also returns the base texture size.

### FFC-012: Array-slice sprite SRVs do not match shader dimensions

**Priority:** P2. **Status:** Open. **Evidence:** Source-reviewed. **Origin:** Inherited.

**Location:** `dx12\shaders\data.hlsli:31-32`; `dx12\dx12_resource.c:10-17,329-337`; `dx12\dx12_draw_device.c:597-607`, `apply_texture_table`; intended slice usage in `dx12\dx12_texture_view.h:8-10`.

**Cause:** Sprite shaders declare `Texture2D` resources. Views of textures whose resource array size exceeds one are created as `TEXTURE2DARRAY`, even when the view selects one slice. Descriptor copying does not make those dimensions compatible.

**Trigger:** Create a two-slice texture, create a view selecting slice 1 with count 1, and draw a sprite through that view.

**Impact:** The shader resource type and SRV dimension disagree. D3D12 resource-dimension diagnostics can be reported, and rendering is not defined by the intended shader contract. The public view description specifically suggests this slice-based sprite use case.

**Fix direction:** Implement compatible array-aware sampling and slice handling, or explicitly reject unsupported sprite views. Selecting one slice alone does not turn an array SRV into a `Texture2D` SRV.

**Regression scenario:** Render distinguishable patterns from separate slices for ordinary and palette sprites. Cover every supported descriptor-binding path, not only descriptor creation.

**Parity evidence:** The original sprite shader declarations and resource view selection have the same mismatch.

### FFC-013: Depthless batching changes draw order

**Priority:** P2. **Status:** Open. **Evidence:** Source-reviewed. **Origin:** Inherited behavior; contract decision required.

**Location:** `dx12\dx12_draw_device.c:697-708,780-783`, `draw_opaque` and `ff_dx12_draw_device_flush`; nullable depth handling in `ff_dx12_draw_device_begin` and bucket state selection.

**Cause:** The accepted `depth == NULL` path still draws opaque buckets in geometry-type order, followed by all transparent instances. Assigned depth values cannot restore submission order when depth testing is disabled.

**Trigger:** Begin without a depth target. Submit an opaque red circle, followed by an overlapping opaque blue rectangle. The rectangle bucket executes before the circle bucket, placing red on top. A transparent object submitted before an overlapping opaque object can likewise be composited afterward.

**Impact:** Overlapping depthless content does not follow painter's order. The public API does not state a nonoverlap restriction for this path.

**Fix direction:** Preserve ordering when depth is absent, or explicitly constrain and enforce the supported use case. Record the chosen compatibility contract when resolving this entry; depth-enabled batching should retain its existing optimization.

**Regression scenario:** Compare overlapping geometry types and mixed opaque/transparent submissions with and without depth, including reversed submission order.

## App, base, and data

### FFC-014: App shutdown deadlocks on synchronous main dispatch

**Priority:** P1. **Status:** Fixed. **Evidence:** Source-reviewed. **Origin:** Unclassified.

**Location:** `app\app.c:83-84`, `ff_app_destroy`; `windows\task.c:64-71,99`, task draining; `windows\dispatch.c:305-314`, `ff_dispatch_send`.

**Cause:** Main-thread destruction waits for task callbacks to finish before flushing the main dispatcher. A callback can be waiting for main to execute its synchronous dispatch. The task-drain wait does not pump that dispatch.

**Trigger:** Initialize an app with an empty window title to isolate shutdown from the main-window message loop. Submit a worker that announces it has started and synchronously sends work to the main dispatcher. Main observes the started event and calls `ff_app_destroy` instead of pumping the dispatcher.

**Impact:** Main waits for the worker; the worker waits for main. Shutdown never returns. Windowed applications can encounter the same dependency once their message loop stops.

**Resolution:** Working tree, not committed. `ff_task_destroy` stops thread-pool submissions under the task lock, keeps shutdown-time inline submissions counted, and flushes the supplied main dispatcher until outstanding tasks reach zero. Passing `NULL` drains without pumping. App teardown now drains and pumps before destroying the main window; it then destroys the window and processes its quit message before tearing down the dispatcher. Late sends remain covered because their worker task stays outstanding until the synchronous send returns.

**Regression coverage:** `app_tests::destroy_pumps_early_and_late_worker_sends` passed in Debug/x64 and Release/x64. It sends synchronously from a worker twice with a delay after the first dispatch, verifies the main window is alive for both callbacks, and submits tasks from dispatch callbacks during shutdown. The test has a 10-second method timeout and a bounded start wait. All 65 tests in the resource, tracker, lifetime, app, and task test groups pass in both configurations.

### FFC-015: Dictionary mutation corrupts borrowed input values

**Priority:** P2. **Status:** Fixed. **Evidence:** Source-reviewed. **Origin:** Unclassified.

**Location:** `data\dict.c:102-109`, `ff_dict_set`; `data\dict.c:26-34`, `internal_ff_dict_add_hash`.

**Cause:** `ff_dict_set` clears matching entries before copying its input value. Clearing shifts surviving values, which can overwrite an input obtained from the same dictionary. Adding during capacity growth can also move or overwrite the input storage before dereferencing it.

**Trigger:** Insert `"a" = 1`, then `"b" = 2`. Call `ff_dict_set(&dict, FF_SVL("a"), ff_dict_get(&dict, FF_SVL("a")))`.

**Impact:** The value of `"a"` becomes 2 instead of remaining 1. This requires neither invalid input on entry nor allocation failure. The API does not prohibit using its returned value as mutation input.

**Resolution:** Working tree, not committed. `ff_dict_set` snapshots a non-null input before clearing matching entries. `internal_ff_dict_add_hash` snapshots its input before growth or writing the new key, protecting both `ff_dict_add` and missing-key insertion through `ff_dict_set`. Values retain shallow ownership, duplicate ordering is unchanged, and null still means removal for `set` and no insertion for `add`. Previously borrowed entry pointers can still be invalidated by mutation; accepting one as input does not make it stable afterward.

**Regression coverage:** `dict_tests::set_preserves_borrowed_values_during_compaction` covers self-assignment, another entry, and duplicate removal while checking survivor values and ordering. `borrowed_values_survive_add_and_set_growth` exercises both APIs at capacities 1, 3, and 8, with verified in-place growth and forced relocation. `set_borrowed_array_preserves_shallow_storage` checks that an aliased array retains its payload pointer, count, and contents. All 243 selected dictionary, immutable-dictionary, and JSON tests pass in Debug/x64 and Release/x64 without skips (2026-09-30).

### FFC-016: Embedding immutable dictionaries breaks payload alignment

**Priority:** P2. **Status:** Fixed. **Evidence:** Source-reviewed. **Origin:** Unclassified.

**Location:** `data\idict.c:318-324`, immutable-child conversion in `build_idict_convert_value`; alignment rejection at `data\idict.c:647-656`.

**Cause:** An existing immutable dictionary is copied at `FF_IDICT_BLOCK_ALIGN`, preserving its bytes and offsets but not necessarily the alignment phase required by its payloads. Internal payload alignment can be stronger than block alignment.

**Trigger:** Create a standalone idict containing a 64-byte-aligned data item, then embed it as the sole child of another dictionary using `ff_value_new_idict`. In the baseline layout, a payload originally at offset 64 can move with its child to parent offset 40, putting the payload at offset 104.

**Impact:** The copied payload is no longer 64-byte aligned. Saving can succeed, but loading with value validation rejects the library's own output. Direct users trusting the stored alignment receive an invalidly aligned pointer.

**Related surface:** Extracting and saving an already-nested immutable dictionary can change its alignment phase too (`build_idict_emit_dict` and the save path). Fix both embedding and extraction/standalone serialization, not just one example offset.

**Fix direction:** Rebuild or place copied immutable content in a way that preserves all payload alignment guarantees, including nested content and standalone saves.

**Regression scenario:** Round-trip over-aligned payloads through immutable embedding, multiple nesting levels, extraction, and standalone saving. Assert actual pointer alignment as well as payload bytes and accepted reloads.

### FFC-017: Embedding an empty immutable dictionary omits its header

**Priority:** P2. **Status:** Open. **Evidence:** Source-reviewed. **Origin:** Unclassified.

**Location:** `data\idict.c:320-324`, immutable-child conversion in `build_idict_convert_value`.

**Cause:** `get_idict_byte_size` returns zero for `ff_idict_empty()`. The builder copies no child header, but still emits an idict value referencing the resulting offset. That offset can be the end of the parent block.

**Trigger:** Put `ff_value_new_idict(ff_idict_empty())` under a `"child"` key and build an immutable dictionary.

**Impact:** The resulting child is not a valid empty dictionary representation. Validated reload fails, and querying the child can read outside the serialized block.

**Fix direction:** Normalize the empty immutable input to a real serialized empty child dictionary, consistent with the supported empty mutable-dictionary path.

**Regression scenario:** Access, save, reload, and convert an empty immutable child. Include multiple empty children and empty children inside arrays, and distinguish a genuinely empty dictionary from an absent value.

### FFC-018: String-builder insertion corrupts aliased source text

**Priority:** P2. **Status:** Open. **Evidence:** Source-reviewed. **Origin:** Unclassified.

**Location:** `base\string_builder.c:142-150`, `ff_string_builder_insert`.

**Cause:** Insertion shifts the destination tail before copying the source view. If that view points into the builder, the shift can change the source bytes. Capacity growth is an additional aliasing consideration, but is not needed for the minimal failure.

**Trigger:** Start with `"abcd"` and sufficient spare capacity. Insert a two-character view at `sb.data + 2` at position zero.

**Impact:** Expected `"cdabcd"`; actual `"ababcd"`. The move changes the source slice from `"cd"` to `"ab"` before the copy. Simply changing the final `memcpy` to `memmove` does not repair that ordering.

**Fix direction:** Preserve aliased input before growth or shifting, using the project's arena conventions. Make the supported aliasing contract consistent across builder mutation functions.

**Regression scenario:** Insert self-substrings before, inside, and after their source range, both with spare capacity and with relocation. Include insertion of the full builder view.

### FFC-019: Dictionary producers and consumers disagree on depth limits

**Priority:** P2. **Status:** Fixed. **Evidence:** Source-reviewed. **Origin:** Unclassified.

**Introduced by:** `9a6abbbbf97ecd9f86a7f809f41fbd6873dffcb6` (`Fixes`).

**Location:** `data\idict.c:299-306`, recursive construction; `data\idict.c:462,620`, conversion and validation limits. The idict limit is 64; the JSON parser permits deeper nesting.

**Cause:** Construction accepts nested values that consumers later reject or replace. Recursive immutable construction has no matching depth limit, while mutable conversion silently returns an empty value beyond its limit.

**Trigger:** Parse a JSON object whose `"a"` value is 65 nested arrays containing the number 1 using `ff_json_parse_idict`. The JSON parser permits up to 256 levels, so this depth is accepted by that layer.

**Impact:** Saving and validated loading reject the accepted structure. Direct conversion through `ff_dict_init_from_idict` can silently replace the excessive-depth scalar with `ff_value_type_empty`, losing data.

**Resolution:** Working tree, not committed. At the user's request, remove all arbitrary recursion-depth limits from JSON parsing, immutable validation, and mutable conversion. Keep structural, alignment, bounds, and forward-array-offset checks. Recursive operations remain subject to available stack and memory; this is not a promise of unlimited nesting.

**Regression coverage:** 512 nested JSON arrays, a 128-level dictionary save/validated-load/mutable-conversion round trip, a deep manually wrapped serialized chain, and 160 alternating array/object pairs carried from JSON through immutable save/load and mutable conversion to the original scalar.

### FFC-020: Log sink configuration changes deadlock inside callbacks

**Priority:** P2. **Status:** Fixed. **Evidence:** Source-reviewed. **Origin:** Unclassified.

**Introduced by:** `9a6abbbbf97ecd9f86a7f809f41fbd6873dffcb6` (`Fixes`).

**Location:** `base\log.c:87-92`, `ff_log_write_v`; exclusive configuration locking in `ff_log_set_sink` and `ff_log_set_type_enabled`.

**Cause:** The sink callback runs while the logging SRW lock is held shared. A callback that changes logging configuration tries to acquire that same lock exclusively on the same thread. SRW locks do not support this upgrade.

**Trigger:** Install a sink that disables its log type or unregisters itself using the public logging API, then write an enabled message.

**Impact:** The logging call deadlocks permanently. The callback contract does not state a prohibition that prevents this otherwise plausible use.

**Resolution:** Working tree, not committed. Remove the logging SRW lock, as requested. Configuration belongs before concurrent logging starts or after it stops; sink cookies must outlive all calls, and sinks synchronize their own mutable state. Single-threaded callbacks may change configuration or log recursively. Concurrent reconfiguration and sink-cookie destruction are deliberately not supported.

**Regression coverage:** A sink queries and disables its type and replaces itself with the previous sink. A separate sink performs bounded recursive logging. Both restore configuration before assertions. The ownership contract is recorded in `base\log.h`.

### FFC-021: JSON decimal parsing depends on the numeric locale

**Priority:** P2. **Status:** Open. **Evidence:** Source-reviewed. **Origin:** Unclassified.

**Location:** `data\json.c`, `json_number_value` and the cached C numeric locale.

**Cause:** JSON tokenization correctly requires a period as the decimal separator, but conversion through `strtod` uses the active CRT numeric locale. The complete-consumption guard then rejects a period that a comma-decimal locale does not recognize.

**Trigger:** Successfully select a comma-decimal `LC_NUMERIC` locale and parse `{"x":1.5}`.

**Impact:** Valid JSON is rejected depending on unrelated application locale configuration. Integer-only inputs can mask the problem.

**Resolution:** Floating-point tokens use `_strtod_l` with a C numeric locale created once through `InitOnceExecuteOnce`. Each parser caches the locale after its first floating-point token, so integer-only parses do not initialize or query it and later floating-point tokens avoid repeated one-time initialization checks. The process locale is never changed. Integer parsing, complete-token validation, overflow rejection, and underflow behavior are preserved.

**Regression coverage:** `json_tests::decimal_and_exponent_numbers_ignore_numeric_locale` parses decimal and exponent values while the current thread uses an available comma-decimal CRT locale, and checks that large integers retain exact `int64` values. JSON tests pass in Debug/x64 and Release/x64.

### FFC-022: Degenerate rectangles can intersect

**Priority:** P2. **Status:** Not a bug. **Evidence:** Disproved. **Origin:** Inherited.

**Location:** `base\rect.h:150-153`, `ff_rect_float_intersects`; empty definition at `base\rect.h:135-138`.

**Finding:** Intersection operates on rectangle geometry, including degenerate lines and points. A horizontal and vertical degenerate rectangle can intersect at a point; a degenerate rectangle can also intersect a positive-area rectangle. This matches the legacy `ff::rect_t::intersects` implementation.

**Contract evidence:** Legacy `ff::rect_t::empty` returns true only when both dimensions are zero. Its strict overlap test returns true for crossing horizontal/vertical degenerate rectangles and for a point inside a positive-area rectangle. The C `ff_rect_float_intersects` uses the same comparisons. C `empty` semantics have been aligned with the legacy implementation.

**Resolution:** No intersection bug. Disjoint intersection results return the zero rectangle as in the legacy implementation. Deflating past the center returns zero rather than reflecting the rectangle into a smaller nonempty rectangle.

**Regression coverage:** `math_types_tests::degenerate_rectangles_intersect_by_shared_geometry`, `touching_rects_do_not_intersect`, and `deflate_past_center_produces_empty` cover degenerate intersections, edge contact, and overshrink behavior.

## Window behavior

### FFC-023: Queued fullscreen requests discard the final desired state

**Priority:** P2. **Status:** Open. **Evidence:** Source-reviewed. **Origin:** Unclassified.

**Location:** `windows\window.c:475-479`, `ff_window_main_set_full_screen`.

**Cause:** The asynchronous setter decides whether to post based on the currently applied window style, without considering pending requests. Execution-time state and requested state can differ until messages are processed.

**Trigger:** Starting windowed, call `ff_window_main_set_full_screen(true)` and then `ff_window_main_set_full_screen(false)` without pumping between them. Process messages afterward.

**Impact:** The second request is discarded because the window is still windowed when it is made. The first queued request subsequently makes it fullscreen, opposite to the final requested state. The reverse sequence has the symmetric failure.

**Fix direction:** Track pending desired state or always enqueue requests for a valid window and let the existing application-time no-op logic decide whether work is needed.

**Regression scenario:** Queue alternating requests without intermediate pumping and assert the final applied state matches the last request in both initial states.

### FFC-024: Window placement mixes workspace and screen coordinates

**Priority:** P2. **Status:** Open. **Evidence:** Source-reviewed. **Origin:** Unclassified.

**Location:** `windows\window.c:101-105`, `current_window_state`; `windows\window.c:216-220`, fullscreen restoration; `windows\window.c:393-415`, saved placement during creation.

**Cause:** `GetWindowPlacement().rcNormalPosition` is workspace-relative for these non-tool top-level windows. The saved rectangle is later passed directly to `SetWindowPos` and `CreateWindowEx`, whose position arguments use screen coordinates.

**Trigger:** Reserve space on the top or left edge with a taskbar/appbar. Position a normal window, toggle fullscreen off/on, or save and recreate the window.

**Impact:** Restored placement shifts by the workspace offset. Repeated save/restore cycles can creep the window away from its intended position. Common bottom-taskbar configurations can conceal the mismatch.

**Fix direction:** Restore through `SetWindowPlacement`, or consistently convert between the two coordinate systems before using screen-coordinate APIs. Preserve monitor and DPI compatibility handling.

**Regression scenario:** Exercise fullscreen restoration and process-style save/recreate with nonzero top/left workspace offsets and multiple monitors. Verify position as well as size.

**Reference:** [WINDOWPLACEMENT documentation](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-windowplacement) explicitly describes the workspace/screen-coordinate distinction and the resulting creeping-window error.

### FFC-025: Message-window class registration races across threads

**Priority:** P2. **Status:** Open. **Evidence:** Source-reviewed. **Origin:** Unclassified.

**Location:** `windows\window.c:439-449`, `ff_window_message_init`.

**Cause:** The `GetClassInfo` followed by `RegisterClass` sequence is not atomic. A second thread can legitimately observe that the class is absent, then lose the registration race. `ERROR_CLASS_ALREADY_EXISTS` is treated as initialization failure rather than a concurrently completed registration.

**Trigger:** Before the message-window class exists, two threads initialize their first thread-local dispatchers concurrently. Both pass the absence check; one registers the class before the other reaches `RegisterClass`.

**Impact:** A valid dispatcher initialization unexpectedly fails, and Debug reports an assertion. Existing registered-class state can hide the race in subsequent runs within the same process.

**Fix direction:** Use one-time synchronized registration or safely handle/recheck the already-registered class case. Do not assume an arbitrary same-name class is compatible without checking the relevant registration contract.

**Regression scenario:** Exercise concurrent first-time initialization in a fresh process, with controlled interleaving if practical. Verify both threads obtain functioning message windows/dispatchers.

## Follow-up review of commit `9a6abbb` (`Fixes`)

FFC-019 and FFC-020 above were also traced to this commit. The following three additional regressions were found during the commit review.

**Resolution validation (2026-09-30):** FFC-019, FFC-020, and FFC-026 through FFC-028 are fixed in the uncommitted working tree. The `ff.test.unit.c` project builds in Debug/x64 and Release/x64. In each configuration, all 249 tests selected from `log_tests`, `json_tests`, `idict_tests`, `dx12_resource_tracker_tests`, `dx12_object_cache_tests`, `format_tests`, `dx12_milestone4_tests`, and `dx12_lifetime_deep_tests` pass without skips. This verifies the corrected behavior; the baseline findings remain source-reviewed rather than claiming executed baseline reproductions. No D3D12 debug-layer validation is claimed. Tracker regressions inspect recorded barrier calls directly, and texture regressions perform GPU readbacks.

### FFC-026: Forgotten-resource barriers inherit replacement-resource state

**Priority:** P1. **Status:** Fixed. **Evidence:** Source-reviewed. **Origin:** Unclassified.

**Introduced by:** `9a6abbbbf97ecd9f86a7f809f41fbd6873dffcb6` (`Fixes`).

**Location:** `dx12\dx12_resource_tracker.c`, `ff_dx12_resource_tracker_forget` and `ff_dx12_resource_tracker_close`; live-entry identity in `dx12\dx12_resource_tracker.h`.

**Cause:** Forgotten first barriers retain the old native resource but look up predecessor state solely by the reusable wrapper address. Closing a list merges live replacement-resource final states into the predecessor before resolving forgotten barriers, allowing a different native allocation's state to satisfy the lookup.

**Trigger:** With an uploaded buffer initially in COMMON, update it on list B, grow it on that same list (reusing the wrapper), then bind the replacement as a vertex buffer. Submit an otherwise empty list A followed by B. B merges the replacement's vertex-buffer state into A and then uses it as the old buffer's before-state.

**Impact:** The old allocation receives a VERTEX_AND_CONSTANT_BUFFER-to-COPY_DEST barrier instead of its valid COMMON transition/promotion. Incorrect barriers can cause debug-layer errors or invalid GPU execution.

**Resolution:** Working tree, not committed. Live entries snapshot native resource identity; predecessor lookups require identity matches, and a reused wrapper resets the predecessor entry before merging replacement state. All first barriers resolve before current final states merge. Forgotten barriers and their COM references remain intact. FFC-002's broader multiple-open-tracker wrapper-lifetime problem is a separate unresolved issue.

**Regression coverage:** Four recorded-barrier cases cover COMMON buffer promotion, preservation of a required forgotten transition, use of a matching predecessor before replacement-state merging, and rejection of a predecessor belonging to another native allocation. Assertions inspect native resource pointers and StateBefore/StateAfter rather than relying on final tracker state alone.

### FFC-027: Pipeline cache rejects embedded root signatures

**Priority:** P2. **Status:** Fixed. **Evidence:** Source-reviewed. **Origin:** Unclassified.

**Introduced by:** `9a6abbbbf97ecd9f86a7f809f41fbd6873dffcb6` (`Fixes`).

**Location:** `dx12\dx12_object_cache.c`, `build_pipeline_state_key`.

**Cause:** Key construction unconditionally requires `pRootSignature` to identify a cached root-signature object. D3D12 permits a null pointer when the supplied shaders embed matching root signatures.

**Trigger:** Request a graphics PSO with matching shader-embedded root signatures and `pRootSignature == NULL`.

**Impact:** A valid descriptor fails key construction and never reaches `CreateGraphicsPipelineState`.

**Resolution:** Working tree, not committed. Encode an absent explicit root as a zero-length root-signature key component, retaining shader bytecode in the key. Explicit non-null roots still require cache membership, and full-content key comparisons remain unchanged.

**Regression coverage:** Create and reuse a real PSO from compiled embedded-root shaders; separately create/reuse its explicit-root form. Check stable null-root hashes, sensitivity to shader content, and rejection of foreign explicit roots.

**Reference:** [Creating a Root Signature](https://learn.microsoft.com/en-us/windows/win32/direct3d12/creating-a-root-signature), "Root Signature in Pipeline State Objects".

### FFC-028: Incomplete format metadata rejects valid textures

**Priority:** P2. **Status:** Fixed. **Evidence:** Source-reviewed. **Origin:** Unclassified.

**Introduced by:** `9a6abbbbf97ecd9f86a7f809f41fbd6873dffcb6` (`Fixes`).

**Location:** `dx12\dx12_texture.c`, `ff_dx12_texture_init` and `ff_dx12_texture_update`; `dx12\dx12_format.c`, format metadata and row-layout helpers.

**Cause:** New upload and optimized-clear guards rely on a convenience format table that omits ordinary D3D12 texture formats, including `DXGI_FORMAT_R16G16B16A16_FLOAT`. Unknown table entries yield zero row sizes and false render-target classification even when the device supports the format.

**Trigger:** Create and upload an RGBA16_FLOAT HDR texture, or request an optimized clear for one.

**Impact:** Uploads fail, render-target flags are omitted, and valid optimized clears are rejected. Other omitted scalar/vector and packed color formats are affected too.

**Resolution:** Working tree, not committed. Expand metadata across conventional scalar/vector, packed-color, and BC1-BC7 formats. Texture creation queries device render-target support rather than relying on the convenience table. Uploads preserve block rows and small-mip block extents, copy only meaningful source bytes into aligned staging rows, and validate pitch, bounds, block alignment, and overflow. Unsupported multi-plane video layouts still return zero from layout helpers; this does not add a planar upload API.

**Regression coverage:** Format-family row-layout and overflow cases; padded-source GPU upload/readback for representative floating-point, normalized, packed and integer formats; BC1-BC7 block rows and 2x2/1x1 mips; invalid row/block-region guards; and optimized HDR target clears checked by GPU readback. Tests compare layout helpers against native `GetCopyableFootprints` output.

## Design constraints to preserve while fixing

- DX12 has one owning thread. Fix GPU queue dependencies and object lifetimes without adding locks to every device object. Keep cross-thread window work on the deferred queue.
- Keep POD public structs, explicit init/destroy, arena allocation, and stable addresses for intrusive or externally referenced objects. POD layout does not imply that ownership can be safely copied.
- Preserve nonblocking behavior for unsubmitted GPU work. A CPU wait is not a safe fallback when the same thread still needs to submit the work that would satisfy it.
- Maintain both wrapper lifetime and underlying GPU/COM lifetime; satisfying one does not satisfy the other.
- Do not classify normal reusable arena capacity as a leak. FFC-007 and FFC-008 concern storage whose pointers are abandoned and cannot be reused.
- Respect explicit GPU-retirement preconditions for low-level descriptors, draw states, and caches. Immediate release under an existing documented retirement precondition was not reported as a separate bug.
- Ordinary texture-content loss on device reset and explicitly deferred graphics features were not counted as these findings. Do not promise complete C++ feature parity merely by closing this backlog.

## New finding template

Copy this section for a new finding, assign the next permanent ID, and add its index row. Replace placeholders; do not leave a speculative issue looking source-confirmed.

### FFC-NNN: Short description of the observable failure

**Priority:** P1 or P2. **Status:** Open. **Evidence:** Source-reviewed or Reproduced. **Origin:** Inherited, Port regression, or Unclassified.

**Location:** Relative paths, current line ranges, and durable function/type names.

**Cause:** Explain the broken invariant or contract, not just the symptom.

**Trigger:** Give the smallest concrete supported input or operation sequence. State assumptions, relevant configuration, and any required concurrency interleaving.

**Impact:** Describe what the caller observes and the affected scope.

**Fix direction:** Describe the required invariant and compatibility constraints without presenting an unimplemented suggestion as a completed fix.

**Regression scenario:** State the observable property that will distinguish a real fix from a success-shaped outcome. Record runtime reproduction details separately when available.

**Resolution:** Add status rationale, owner if useful, commit/issue references, regression evidence, and remaining limitations when work begins or the finding is closed.
