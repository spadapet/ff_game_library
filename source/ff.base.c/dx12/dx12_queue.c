#include "pch.h"
#include "base/assert.h"
#include "base/string.h"
#include "dx12/dx12_commands.h"
#include "dx12/dx12_globals.h"
#include "dx12/dx12_queue.h"
#include "dx12/dx12_residency.h"

void ff_dx12_residency_set_clear(ff_dx12_residency_set* set)
{
    FF_CHECK_RET(set);

    if (set->slots)
    {
        memset(set->slots, 0, sizeof(ff_dx12_residency_data*) * set->capacity);
    }

    set->count = 0;
}

static size_t residency_set_slot(ff_dx12_residency_data** slots, size_t capacity, ff_dx12_residency_data* data)
{
    size_t index = (((size_t)(uintptr_t)data) >> 4) & (capacity - 1);

    while (slots[index] && slots[index] != data)
    {
        index = (index + 1) & (capacity - 1);
    }

    return index;
}

// Keeps the table at most half full so the linear probe above stays short and always terminates.
static bool residency_set_grow(ff_arena* arena, ff_dx12_residency_set* set)
{
    const size_t old_capacity = set->capacity;
    const size_t new_capacity = old_capacity ? old_capacity * 2 : FF_DX12_RESIDENCY_SET_MIN;

    ff_dx12_residency_data** new_slots = ff_arena_alloc_type(arena, ff_dx12_residency_data*, new_capacity);
    FF_ASSERT_RET_VAL(new_slots, false);

    memset(new_slots, 0, sizeof(ff_dx12_residency_data*) * new_capacity);

    for (size_t i = 0; i < old_capacity; i++)
    {
        if (set->slots[i])
        {
            new_slots[residency_set_slot(new_slots, new_capacity, set->slots[i])] = set->slots[i];
        }
    }

    set->slots = new_slots;
    set->capacity = new_capacity;

    return true;
}

bool ff_dx12_residency_set_add(ff_arena* arena, ff_dx12_residency_set* set, ff_dx12_residency_data* data)
{
    FF_CHECK_RET_VAL(arena && set && data, false);

    if ((set->count + 1) * 2 > set->capacity)
    {
        FF_CHECK_RET_VAL(residency_set_grow(arena, set), false);
    }

    const size_t index = residency_set_slot(set->slots, set->capacity, data);

    if (!set->slots[index])
    {
        set->slots[index] = data;
        set->count++;
    }

    return true;
}

static size_t residency_set_gather(const ff_dx12_residency_set* set, ff_dx12_residency_data** out, size_t out_max, size_t out_count)
{
    for (size_t i = 0; i < set->capacity && out_count < out_max; i++)
    {
        if (set->slots[i])
        {
            bool found = false;
            for (size_t j = 0; j < out_count && !found; j++)
            {
                found = (out[j] == set->slots[i]);
            }

            if (!found)
            {
                out[out_count++] = set->slots[i];
            }
        }
    }

    return out_count;
}

static void allocator_list_push(ff_dx12_queue* queue, ff_dx12_queue_allocator_list* list,
    ID3D12CommandAllocator* allocator, ff_dx12_fence_value fence_value)
{
    ff_dx12_queue_allocator_node* node = queue->allocator_nodes_free;
    if (node)
    {
        queue->allocator_nodes_free = node->next;
    }
    else
    {
        node = ff_arena_alloc_type(&queue->arena, ff_dx12_queue_allocator_node, 1);
        FF_ASSERT_RET(node);
    }

    node->next = NULL;
    node->allocator = allocator;
    node->fence_value = fence_value;

    if (list->tail)
    {
        list->tail->next = node;
    }
    else
    {
        list->head = node;
    }

    list->tail = node;
}

// Pops the oldest allocator only when the GPU has finished with it. The list is in FIFO order,
// so if the front isn't complete then nothing behind it is either.
static ID3D12CommandAllocator* allocator_list_pop_complete(ff_dx12_queue* queue, ff_dx12_queue_allocator_list* list)
{
    ff_dx12_queue_allocator_node* node = list->head;
    FF_CHECK_RET_VAL(node && ff_dx12_fence_value_complete(node->fence_value), NULL);

    list->head = node->next;
    if (!list->head)
    {
        list->tail = NULL;
    }

    ID3D12CommandAllocator* allocator = node->allocator;
    node->allocator = NULL;
    node->next = queue->allocator_nodes_free;
    queue->allocator_nodes_free = node;

    return allocator;
}

static void allocator_list_release_all(ff_dx12_queue* queue, ff_dx12_queue_allocator_list* list)
{
    for (ff_dx12_queue_allocator_node* node = list->head; node; )
    {
        ff_dx12_queue_allocator_node* next = node->next;

        if (node->allocator)
        {
            ID3D12CommandAllocator_Release(node->allocator);
            node->allocator = NULL;
        }

        node->next = queue->allocator_nodes_free;
        queue->allocator_nodes_free = node;
        node = next;
    }

    list->head = NULL;
    list->tail = NULL;
}

static ID3D12CommandAllocator* new_allocator(ff_dx12_queue* queue, ff_dx12_queue_allocator_list* list, ff_wstring_view suffix)
{
    ID3D12CommandAllocator* allocator = allocator_list_pop_complete(queue, list);

    if (allocator)
    {
        FF_VERIFY_HR(ID3D12CommandAllocator_Reset(allocator));
        return allocator;
    }

    FF_ASSERT_HR_RET_VAL(ID3D12Device6_CreateCommandAllocator(ff_dx12_device(), queue->type,
        &IID_ID3D12CommandAllocator, (void**)&allocator), NULL);

    wchar_t name[128];
    _snwprintf_s(name, _countof(name), _TRUNCATE, L"%s %s %d", queue->name, suffix.data, queue->allocator_counter++);
    ID3D12CommandAllocator_SetName(allocator, name);

    return allocator;
}

static void command_cache_destroy(ff_dx12_command_cache* cache)
{
    FF_CHECK_RET(cache);

    if (cache->list)
    {
        ID3D12GraphicsCommandList1_Release(cache->list);
        cache->list = NULL;
    }

    if (cache->list_before)
    {
        ID3D12GraphicsCommandList_Release(cache->list_before);
        cache->list_before = NULL;
    }

    if (cache->allocator)
    {
        ID3D12CommandAllocator_Release(cache->allocator);
        cache->allocator = NULL;
    }

    if (cache->allocator_before)
    {
        ID3D12CommandAllocator_Release(cache->allocator_before);
        cache->allocator_before = NULL;
    }

    ff_dx12_resource_tracker_destroy(&cache->resource_tracker);
    ff_dx12_fence_destroy(&cache->fence);
    ff_dx12_residency_set_clear(&cache->residency_set);
}

// Takes a recycled cache whose lists are already reset, or builds a new one.
static ff_dx12_command_cache* command_cache_acquire(ff_dx12_queue* queue)
{
    for (ff_dx12_command_cache** it = &queue->caches; *it; it = &(*it)->next)
    {
        if (!(*it)->needs_reset)
        {
            ff_dx12_command_cache* cache = *it;
            *it = cache->next;
            cache->next = NULL;
            return cache;
        }
    }

    ff_dx12_command_cache* cache = ff_arena_alloc_type(&queue->arena, ff_dx12_command_cache, 1);
    FF_ASSERT_RET_VAL(cache, NULL);

    *cache = (ff_dx12_command_cache){ 0 };
    ff_dx12_resource_tracker_init(&cache->resource_tracker);

    ff_arena_declare_stack(name_arena, 256);
    ff_string_view queue_name = ff_wide_to_utf8(ff_wz_view(queue->name), &name_arena, true);
    char fence_name[128];
    _snprintf_s(fence_name, _countof(fence_name), _TRUNCATE, "%.*s fence", FF_SV_FORMAT(queue_name));
    bool fence_ok = ff_dx12_fence_init(&cache->fence, ff_sz_view(fence_name), 0);
    ff_arena_destroy(&name_arena);

    if (!fence_ok)
    {
        command_cache_destroy(cache);
        return NULL;
    }

    ff_dx12_fence_set_owner_queue(&cache->fence, queue->command_queue);

    cache->allocator = new_allocator(queue, &queue->allocators, FF_WSVL(L"allocator"));
    cache->allocator_before = new_allocator(queue, &queue->allocators_before, FF_WSVL(L"allocator before"));

    if (!cache->allocator || !cache->allocator_before ||
        FAILED(ID3D12Device6_CreateCommandList(ff_dx12_device(), 0, queue->type, cache->allocator, NULL,
            &IID_ID3D12GraphicsCommandList1, (void**)&cache->list)) ||
        FAILED(ID3D12Device6_CreateCommandList(ff_dx12_device(), 0, queue->type, cache->allocator_before, NULL,
            &IID_ID3D12GraphicsCommandList, (void**)&cache->list_before)))
    {
        command_cache_destroy(cache);
        FF_DEBUG_FAIL_RET_VAL(NULL);
    }

    wchar_t name[128];
    const int counter = queue->list_counter++;
    _snwprintf_s(name, _countof(name), _TRUNCATE, L"%s commands %d", queue->name, counter);
    ID3D12GraphicsCommandList1_SetName(cache->list, name);
    _snwprintf_s(name, _countof(name), _TRUNCATE, L"%s commands before %d", queue->name, counter);
    ID3D12GraphicsCommandList_SetName(cache->list_before, name);

    return cache;
}

// The old code did this on a thread pool task and gated hand-out on an event. Single-threaded
// v1 resets inline right after ExecuteCommandLists, which needs no event and no waiting.
static void command_cache_reset_lists(ff_dx12_queue* queue, ff_dx12_command_cache* cache)
{
    FF_CHECK_RET(cache && cache->needs_reset);

    cache->allocator = new_allocator(queue, &queue->allocators, FF_WSVL(L"allocator"));
    cache->allocator_before = new_allocator(queue, &queue->allocators_before, FF_WSVL(L"allocator before"));
    FF_ASSERT_RET(cache->allocator && cache->allocator_before);

    FF_VERIFY_HR(ID3D12GraphicsCommandList1_Reset(cache->list, cache->allocator, NULL));
    FF_VERIFY_HR(ID3D12GraphicsCommandList_Reset(cache->list_before, cache->allocator_before, NULL));

    cache->needs_reset = false;
}

bool ff_dx12_queue_init(ff_dx12_queue* queue, ff_string_view name, D3D12_COMMAND_LIST_TYPE type)
{
    FF_ASSERT_RET_VAL(queue, false);

    *queue = (ff_dx12_queue){ 0 };
    ff_arena_init_heap_local(&queue->arena, 4096);
    queue->type = type;

    ff_arena_declare_stack(name_arena, 256);
    ff_wstring_view wide_name = ff_utf8_to_wide(name, &name_arena, true);
    wcsncpy_s(queue->name, _countof(queue->name), wide_name.data, _TRUNCATE);
    ff_arena_destroy(&name_arena);

    char idle_name_buffer[128];
    _snprintf_s(idle_name_buffer, _countof(idle_name_buffer), _TRUNCATE, "%.*s idle fence", FF_SV_FORMAT(name));
    bool fence_ok = ff_dx12_fence_init(&queue->idle_fence, ff_sz_view(idle_name_buffer), 0);
    if (!fence_ok)
    {
        ff_dx12_queue_destroy(queue);
        return false;
    }

    const D3D12_COMMAND_QUEUE_DESC desc = { .Type = type };
    if (FAILED(ID3D12Device6_CreateCommandQueue(ff_dx12_device(), &desc, &IID_ID3D12CommandQueue, (void**)&queue->command_queue)))
    {
        ff_dx12_queue_destroy(queue);
        FF_DEBUG_FAIL_RET_VAL(false);
    }

    ID3D12CommandQueue_SetName(queue->command_queue, queue->name);
    ff_dx12_fence_set_owner_queue(&queue->idle_fence, queue->command_queue);

    return true;
}

void ff_dx12_queue_destroy(ff_dx12_queue* queue)
{
    FF_CHECK_RET(queue);

    if (queue->command_queue && ff_dx12_device_valid())
    {
        ff_dx12_queue_wait_for_idle(queue);
    }

    for (ff_dx12_command_cache* cache = queue->caches; cache; cache = cache->next)
    {
        command_cache_destroy(cache);
    }
    queue->caches = NULL;

    allocator_list_release_all(queue, &queue->allocators);
    allocator_list_release_all(queue, &queue->allocators_before);
    queue->allocator_nodes_free = NULL;

    ff_dx12_fence_destroy(&queue->idle_fence);

    if (queue->command_queue)
    {
        ID3D12CommandQueue_Release(queue->command_queue);
        queue->command_queue = NULL;
    }

    ff_arena_destroy(&queue->arena);
}

bool ff_dx12_queue_valid(const ff_dx12_queue* queue)
{
    return queue && queue->command_queue;
}

ID3D12CommandQueue* ff_dx12_queue_command_queue(ff_dx12_queue* queue)
{
    return queue ? queue->command_queue : NULL;
}

D3D12_COMMAND_LIST_TYPE ff_dx12_queue_type(const ff_dx12_queue* queue)
{
    return queue ? queue->type : D3D12_COMMAND_LIST_TYPE_DIRECT;
}

void ff_dx12_queue_wait_for_idle(ff_dx12_queue* queue)
{
    FF_CHECK_RET(ff_dx12_queue_valid(queue));

    ff_dx12_fence_value value = ff_dx12_fence_signal(&queue->idle_fence, queue->command_queue);
    ff_dx12_fence_value_wait(value, NULL);
}

bool ff_dx12_queue_new_commands(ff_dx12_queue* queue, ff_dx12_commands* commands)
{
    FF_ASSERT_RET_VAL(ff_dx12_queue_valid(queue) && commands, false);

    ff_dx12_command_cache* cache = command_cache_acquire(queue);
    FF_ASSERT_RET_VAL(cache, false);

    *commands = (ff_dx12_commands){ 0 };
    commands->queue = queue;
    commands->cache = cache;
    commands->type = queue->type;
    ff_dx12_fence_values_init_arena(&commands->wait_before_execute, &queue->arena);

    return true;
}

ff_dx12_fence_value ff_dx12_queue_execute(ff_dx12_queue* queue, ff_dx12_commands* commands)
{
    FF_ASSERT_RET_VAL(queue && commands, ((ff_dx12_fence_value) { 0 }));

    ff_dx12_fence_value fence_value = ff_dx12_commands_next_fence_value(commands);
    ff_dx12_queue_execute_many(queue, &commands, 1);

    return fence_value;
}

void ff_dx12_queue_execute_many(ff_dx12_queue* queue, ff_dx12_commands** commands, size_t count)
{
    FF_CHECK_RET(ff_dx12_queue_valid(queue) && commands);
    FF_CHECK_RET(count);

    uint8_t temp_buffer[4096];
    ff_arena temp;
    ff_arena_init_external(&temp, temp_buffer, sizeof(temp_buffer), 4096);

    ff_dx12_commands** valid = ff_arena_alloc_type(&temp, ff_dx12_commands*, count);
    size_t valid_count = 0;

    if (valid)
    {
        for (size_t i = 0; i < count; i++)
        {
            if (commands[i] && ff_dx12_commands_valid(commands[i]))
            {
                valid[valid_count++] = commands[i];
            }
        }
    }

    if (!valid_count)
    {
        ff_arena_destroy(&temp);
        return;
    }

    ff_dx12_fence_values wait_before_execute;
    ff_dx12_fence_values_init_arena(&wait_before_execute, &temp);

    for (size_t i = 0; i < valid_count; i++)
    {
        ff_dx12_commands* prev = i ? valid[i - 1] : NULL;
        ff_dx12_commands* next = (i + 1 < valid_count) ? valid[i + 1] : NULL;
        ff_dx12_commands_close_lists(valid[i], prev, next, &wait_before_execute);
    }

    size_t residency_max = 0;
    for (size_t i = 0; i < valid_count; i++)
    {
        residency_max += valid[i]->cache->residency_set.count;
    }

    ID3D12CommandList** dx12_lists = ff_arena_alloc_type(&temp, ID3D12CommandList*, valid_count * 2);
    ff_dx12_command_cache** caches = ff_arena_alloc_type(&temp, ff_dx12_command_cache*, valid_count);
    ff_dx12_residency_data** residency_set = residency_max ? ff_arena_alloc_type(&temp, ff_dx12_residency_data*, residency_max) : NULL;

    if (!dx12_lists || !caches || (residency_max && !residency_set))
    {
        ff_arena_destroy(&temp);
        FF_DEBUG_FAIL_RET();
    }

    size_t dx12_list_count = 0;
    size_t residency_count = 0;
    size_t cache_count = 0;
    ff_dx12_fence_value next_fence_value = { 0 };

    // Each cache owns a distinct fence, so these are signaled directly rather than gathered into
    // an ff_dx12_fence_values: that set dedupes by fence and has a fixed capacity, and its
    // overflow path blocks on the oldest entry, which here has not been signaled yet.
    ff_dx12_fence_value* signal_values = ff_arena_alloc_type(&temp, ff_dx12_fence_value, valid_count);
    size_t signal_count = 0;

    if (!signal_values)
    {
        ff_arena_destroy(&temp);
        FF_DEBUG_FAIL_RET();
    }

    for (size_t i = 0; i < valid_count; i++)
    {
        next_fence_value = ff_dx12_commands_next_fence_value(valid[i]);
        signal_values[signal_count++] = next_fence_value;

        ff_dx12_command_cache* cache = ff_dx12_commands_take_cache(valid[i]);
        FF_ASSERT(cache);
        if (!cache)
        {
            break;
        }

        // list_before carries the barriers that must run ahead of the recorded work.
        dx12_lists[dx12_list_count++] = (ID3D12CommandList*)cache->list_before;
        dx12_lists[dx12_list_count++] = (ID3D12CommandList*)cache->list;

        residency_count = residency_set_gather(&cache->residency_set, residency_set, residency_max, residency_count);
        ff_dx12_residency_set_clear(&cache->residency_set);

        allocator_list_push(queue, &queue->allocators, cache->allocator, next_fence_value);
        allocator_list_push(queue, &queue->allocators_before, cache->allocator_before, next_fence_value);
        cache->allocator = NULL;
        cache->allocator_before = NULL;

        caches[cache_count++] = cache;
    }

    bool all_resident = ff_dx12_make_resident(residency_set, residency_count, next_fence_value, &wait_before_execute);
    ff_dx12_fence_values_wait(&wait_before_execute, queue->command_queue);

    if (all_resident && dx12_list_count)
    {
        ID3D12CommandQueue_ExecuteCommandLists(queue->command_queue, (UINT)dx12_list_count, dx12_lists);
    }

    for (size_t i = 0; i < signal_count; i++)
    {
        ff_dx12_fence_value_signal(signal_values[i], queue->command_queue);
    }
    for (size_t i = 0; i < cache_count; i++)
    {
        command_cache_reset_lists(queue, caches[i]);

        caches[i]->next = queue->caches;
        queue->caches = caches[i];
    }

    ff_arena_destroy(&temp);
}
