#pragma once

#include "../source/ff.dx12/pch.h"
#include "../source/ff.dx12/source/access.h"
#include "../source/ff.dx12/source/buffer.h"
#include "../source/ff.dx12/source/commands.h"
#include "../source/ff.dx12/source/depth.h"
#include "../source/ff.dx12/source/descriptor_allocator.h"
#include "../source/ff.dx12/source/descriptor_range.h"
#include "../source/ff.dx12/source/device_reset_priority.h"
#include "../source/ff.dx12/source/draw_device.h"
#include "../source/ff.dx12/source/fence.h"
#include "../source/ff.dx12/source/fence_value.h"
#include "../source/ff.dx12/source/fence_values.h"
#include "../source/ff.dx12/source/globals.h"
#include "../source/ff.dx12/source/gpu_event.h"
#include "../source/ff.dx12/source/heap.h"
#include "../source/ff.dx12/source/init.h"
#include "../source/ff.dx12/source/mem_allocator.h"
#include "../source/ff.dx12/source/mem_range.h"
#include "../source/ff.dx12/source/object_cache.h"
#include "../source/ff.dx12/source/queue.h"
#include "../source/ff.dx12/source/queues.h"
#include "../source/ff.dx12/source/residency.h"
#include "../source/ff.dx12/source/resource.h"
#include "../source/ff.dx12/source/resource_state.h"
#include "../source/ff.dx12/source/resource_tracker.h"
#include "../source/ff.dx12/source/target_access.h"
#include "../source/ff.dx12/source/target_texture.h"
#include "../source/ff.dx12/source/target_window.h"
#include "../source/ff.dx12/source/texture.h"
#include "../source/ff.dx12/source/texture_util.h"
#include "../source/ff.dx12/source/texture_view.h"
#include "../source/ff.dx12/source/texture_view_access.h"

#pragma comment(lib, "d3d12.lib")

#if defined(_WIN64) && PROFILE_APP
#pragma comment(lib, "WinPixEventRuntime.lib")
#endif
