#pragma once

#include "../source/ff.base/pch.h"
#include "../source/ff.base/init.h"

#include "../source/ff.base/base/assert.h"
#include "../source/ff.base/base/constants.h"
#include "../source/ff.base/base/log.h"
#include "../source/ff.base/base/math.h"
#include "../source/ff.base/base/memory.h"
#include "../source/ff.base/base/stable_hash.h"
#include "../source/ff.base/base/string.h"

#include "../source/ff.base/data_persist/compression.h"
#include "../source/ff.base/data_persist/data.h"
#include "../source/ff.base/data_persist/dict.h"
#include "../source/ff.base/data_persist/dict_visitor.h"
#include "../source/ff.base/data_persist/file.h"
#include "../source/ff.base/data_persist/filesystem.h"
#include "../source/ff.base/data_persist/json_persist.h"
#include "../source/ff.base/data_persist/json_tokenizer.h"
#include "../source/ff.base/data_persist/persist.h"
#include "../source/ff.base/data_persist/saved_data.h"
#include "../source/ff.base/data_persist/stream.h"

#include "../source/ff.base/data_value/bool_v.h"
#include "../source/ff.base/data_value/data_v.h"
#include "../source/ff.base/data_value/dict_v.h"
#include "../source/ff.base/data_value/double_v.h"
#include "../source/ff.base/data_value/fixed_v.h"
#include "../source/ff.base/data_value/float_v.h"
#include "../source/ff.base/data_value/int_v.h"
#include "../source/ff.base/data_value/null_v.h"
#include "../source/ff.base/data_value/point_v.h"
#include "../source/ff.base/data_value/rect_v.h"
#include "../source/ff.base/data_value/resource_object_v.h"
#include "../source/ff.base/data_value/resource_v.h"
#include "../source/ff.base/data_value/saved_data_v.h"
#include "../source/ff.base/data_value/size_v.h"
#include "../source/ff.base/data_value/string_v.h"
#include "../source/ff.base/data_value/uuid_v.h"
#include "../source/ff.base/data_value/value.h"
#include "../source/ff.base/data_value/value_allocator.h"
#include "../source/ff.base/data_value/value_ptr.h"
#include "../source/ff.base/data_value/value_traits.h"
#include "../source/ff.base/data_value/value_type.h"
#include "../source/ff.base/data_value/value_vector_v.h"
#include "../source/ff.base/data_value/value_vector_base.h"

#include "../source/ff.base/resource/auto_resource.h"
#include "../source/ff.base/resource/global_resources.h"
#include "../source/ff.base/resource/resource.h"
#include "../source/ff.base/resource/resource_file.h"
#include "../source/ff.base/resource/resource_load.h"
#include "../source/ff.base/resource/resource_load_context.h"
#include "../source/ff.base/resource/resource_object_base.h"
#include "../source/ff.base/resource/resource_object_factory_base.h"
#include "../source/ff.base/resource/resource_object_provider.h"
#include "../source/ff.base/resource/resource_objects.h"
#include "../source/ff.base/resource/resource_value_provider.h"
#include "../source/ff.base/resource/resource_values.h"

#include "../source/ff.base/thread/co_awaiters.h"
#include "../source/ff.base/thread/co_exceptions.h"
#include "../source/ff.base/thread/co_task.h"
#include "../source/ff.base/thread/thread_dispatch.h"
#include "../source/ff.base/thread/thread_pool.h"

#include "../source/ff.base/types/fixed.h"
#include "../source/ff.base/types/flags.h"
#include "../source/ff.base/types/frame_allocator.h"
#include "../source/ff.base/types/intrusive_ptr.h"
#include "../source/ff.base/types/perf_timer.h"
#include "../source/ff.base/types/point.h"
#include "../source/ff.base/types/pool_allocator.h"
#include "../source/ff.base/types/push_back.h"
#include "../source/ff.base/types/rect.h"
#include "../source/ff.base/types/scope_exit.h"
#include "../source/ff.base/types/signal.h"
#include "../source/ff.base/types/stack_vector.h"
#include "../source/ff.base/types/stash.h"
#include "../source/ff.base/types/timer.h"
#include "../source/ff.base/types/uuid.h"

#include "../source/ff.base/windows/win32.h"
#include "../source/ff.base/windows/window.h"
#include "../source/ff.base/windows/win_handle.h"
#include "../source/ff.base/windows/win_msg.h"

#pragma comment(lib, "version.lib")
#pragma comment(lib, "shcore.lib")
